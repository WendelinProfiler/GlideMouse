import AppKit
import IOBluetooth
import Carbon
import ObjectiveC
import CoreGraphics
import Darwin

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var item: NSStatusItem!
    private var agent: Process?
    private var busy = false
    private var refreshing = false
    private var timer: Timer?
    private var guardTimer: DispatchSourceTimer?
    private var activeAlert: NSAlert?
    private var connected = false
    private var waitingForInput = false
    private var powerObserver: NSObjectProtocol?
    private var manualQuit = false
    private var shutdownReleaseStarted = false
    private var shutdownReleaseFinished = false
    private var terminationReplyPending = false
    private var terminationSignals: [DispatchSourceSignal] = []
    private var signalTermination = false
    private var connectionWatch: IOBluetoothUserNotification?
    private var lastReleaseRequest = ""
    private var hotKey: EventHotKeyRef?
    private var hotKeyHandler: EventHandlerRef?
    private let bluetoothQueue = DispatchQueue(label: "org.glidemouse.switch.release")
    private let heartbeatQueue = DispatchQueue(label: "org.glidemouse.switch.heartbeat")
    private var folder: URL { FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/GlideMouseSwitch") }
    private var released: Bool { (try? String(contentsOf: folder.appendingPathComponent("released"), encoding: .utf8).trimmingCharacters(in: .whitespacesAndNewlines)) == "released" }
    private var targetAddress: String {
        guard let data = try? Data(contentsOf: folder.appendingPathComponent("settings.json")), let config = try? JSONSerialization.jsonObject(with: data) as? [String: Any], let raw = config["Address"] as? String else { return "" }
        return normalized(raw)
    }
    private func normalized(_ text: String) -> String { text.lowercased().filter { "0123456789abcdef".contains($0) } }
    private var runtime: URL {
        let installed = folder.appendingPathComponent("agent/GlideMouseSwitch")
        return FileManager.default.isExecutableFile(atPath: installed.path) ? installed : Bundle.main.bundleURL.appendingPathComponent("Contents/Resources/runtime/GlideMouseSwitch")
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        guard configureIfNeeded() else { Darwin.exit(0) }
        item = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        item.button?.target = self
        item.button?.action = #selector(clicked)
        item.button?.sendAction(on: [.leftMouseUp, .rightMouseUp])
        draw()
        installHotKey()
        for number in [SIGTERM, SIGINT] {
            Darwin.signal(number, SIG_IGN)
            let source = DispatchSource.makeSignalSource(signal: number, queue: .main)
            source.setEventHandler { [weak self] in
                NSLog("GlideMouse launch-agent termination signal %d", number)
                guard let self else { return }
                self.signalTermination = true
                self.beginPowerOffRelease()
                DispatchQueue.main.asyncAfter(deadline: .now() + 4) { Darwin.exit(0) }
            }
            source.resume()
            terminationSignals.append(source)
        }
        powerObserver = NSWorkspace.shared.notificationCenter.addObserver(forName: NSWorkspace.willPowerOffNotification, object: nil, queue: .main) { [weak self] _ in self?.beginPowerOffRelease() }
        // The framework waits for its CoreBluetooth coordinator during first
        // initialization. Keep the app's main run loop alive while it does so.
        bluetoothQueue.async {
            let watch = IOBluetoothDevice.register(forConnectNotifications: self, selector: #selector(self.deviceConnected(_:device:)))
            DispatchQueue.main.async { self.connectionWatch = watch; self.updateGuard() }
        }
        let child = Process()
        child.executableURL = runtime; child.arguments = ["serve"]
        child.standardOutput = FileHandle.nullDevice; child.standardError = FileHandle.nullDevice
        do { try child.run(); agent = child }
        catch { show("Could not start Host Switch", error.localizedDescription) }
        guardTimer = DispatchSource.makeTimerSource(queue: heartbeatQueue)
        guardTimer?.schedule(deadline: .now(), repeating: 2)
        guardTimer?.setEventHandler { [weak self] in self?.updateGuard() }
        guardTimer?.resume()
        timer = Timer(timeInterval: 4, repeats: true) { [weak self] _ in self?.refresh() }
        RunLoop.main.add(timer!, forMode: .common)
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) { self.refresh() }
    }
    private func setupCommand(_ arguments: [String]) -> (Bool, String) {
        let child = Process(), output = Pipe()
        child.executableURL = runtime; child.arguments = arguments
        child.standardOutput = output; child.standardError = FileHandle.nullDevice
        do {
            try child.run()
            let data = output.fileHandleForReading.readDataToEndOfFile()
            child.waitUntilExit()
            return (child.terminationStatus == 0, String(data: data, encoding: .utf8) ?? "")
        } catch { return (false, error.localizedDescription) }
    }
    private func configureIfNeeded() -> Bool {
        let settings = folder.appendingPathComponent("settings.json")
        if let data = try? Data(contentsOf: settings), let config = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
           !(config["Peer"] as? String ?? "").isEmpty, !(config["PeerPin"] as? String ?? "").isEmpty { return true }
        let form = NSAlert()
        form.messageText = "Set up GlideMouse Switch"
        form.informativeText = "Run Setup-Windows.cmd from the optional Windows Switch add-on first. Copy the mouse address and public fingerprint shown there. This one-time setup uses your local network; it does not use SSH."
        form.addButton(withTitle: "Set up"); form.addButton(withTitle: "Cancel")
        let panel = NSView(frame: NSRect(x: 0, y: 0, width: 390, height: 150))
        var fields: [NSTextField] = []
        for (index, label) in ["Mouse Bluetooth address", "Windows IP or hostname", "Windows public fingerprint"].enumerated() {
            let y = CGFloat(104-index*48)
            let title = NSTextField(labelWithString: label); title.frame = NSRect(x: 0, y: y+25, width: 390, height: 18); panel.addSubview(title)
            let field = NSTextField(frame: NSRect(x: 0, y: y, width: 390, height: 24)); panel.addSubview(field); fields.append(field)
        }
        fields[0].stringValue = targetAddress
        form.accessoryView = panel
        NSApp.activate(ignoringOtherApps: true)
        guard form.runModal() == .alertFirstButtonReturn else { return false }
        let address = normalized(fields[0].stringValue)
        let peer = fields[1].stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        let pin = fields[2].stringValue.replacingOccurrences(of: " ", with: "").uppercased()
        guard address.count == 12, pin.count == 64, pin.allSatisfy({ "0123456789ABCDEF".contains($0) }), !peer.isEmpty else {
            let error = NSAlert(); error.messageText = "Setup values are incomplete"; error.informativeText = "Use the exact address and fingerprint displayed by Windows setup, then open the app again."; error.runModal(); return false
        }
        if !FileManager.default.fileExists(atPath: settings.path) {
            let backend = folder.appendingPathComponent("blueutil-refresh.sh").path
            let initialized = setupCommand(["init",address,backend])
            if !initialized.0 { let error=NSAlert(); error.messageText="Could not initialize Switch"; error.informativeText=initialized.1; error.runModal(); return false }
        } else if address != targetAddress {
            let error=NSAlert(); error.messageText="Existing mouse settings were preserved"; error.informativeText="This installation is already configured for a different mouse."; error.runModal(); return false
        }
        let trusted = setupCommand(["trust",peer,pin])
        guard trusted.0 else { let error=NSAlert(); error.messageText="Could not save Windows trust"; error.informativeText=trusted.1; error.runModal(); return false }
        let identity = setupCommand(["identity"])
        guard identity.0, let data = identity.1.data(using: .utf8), let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any], let publicPin = json["pin"] as? String else { return false }
        NSPasteboard.general.clearContents(); NSPasteboard.general.setString(publicPin,forType:.string)
        let done=NSAlert(); done.messageText="Finish on Windows"; done.informativeText="Your Mac public fingerprint was copied to the clipboard. Paste it into Windows setup. On Windows, enter this Mac's IP address or hostname. Both apps must be allowed to use Bluetooth and the local network.\n\nMac public fingerprint:\n"+publicPin; done.runModal()
        return true
    }
    func applicationWillTerminate(_ notification: Notification) {
        if let powerObserver { NSWorkspace.shared.notificationCenter.removeObserver(powerObserver) }
        timer?.invalidate()
        guardTimer?.cancel()
        if let hotKey { UnregisterEventHotKey(hotKey) }
        if let hotKeyHandler { RemoveEventHandler(hotKeyHandler) }
        connectionWatch?.unregister()
        try? FileManager.default.removeItem(at: folder.appendingPathComponent("guard-ready.json"))
        if agent?.isRunning == true { agent?.terminate() }
    }
    private func beginPowerOffRelease() {
        guard !shutdownReleaseStarted else { return }
        shutdownReleaseStarted = true
        timer?.invalidate()
        // launchd may stop both parent and child. Do not spawn a new CLI or
        // wait for managed Bluetooth initialization while the session ends.
        if agent?.isRunning == true { agent?.terminate() }
        NSLog("GlideMouse native shutdown release started")
        let expires = Date().timeIntervalSince1970 + 2.8
        bluetoothQueue.async {
            let result = self.removeConfiguredDevice(expires: expires)
            if result.completed {
                try? "released".write(to: self.folder.appendingPathComponent("released"), atomically: true, encoding: .utf8)
                try? FileManager.default.removeItem(at: self.folder.appendingPathComponent("pending-mac-bond"))
            }
            NSLog("GlideMouse native shutdown release completed=%@", result.completed ? "true" : "false")
            DispatchQueue.main.async {
                self.shutdownReleaseFinished = true
                if self.signalTermination { Darwin.exit(0) }
                if self.terminationReplyPending {
                    self.terminationReplyPending = false
                    NSApp.reply(toApplicationShouldTerminate: true)
                }
            }
        }
    }
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        if manualQuit || shutdownReleaseFinished { return .terminateNow }
        terminationReplyPending = true
        beginPowerOffRelease()
        DispatchQueue.main.asyncAfter(deadline: .now() + 4) {
            if self.terminationReplyPending {
                self.terminationReplyPending = false
                sender.reply(toApplicationShouldTerminate: true)
            }
        }
        return .terminateLater
    }
    private func installHotKey() {
        var event = EventTypeSpec(eventClass: OSType(kEventClassKeyboard), eventKind: UInt32(kEventHotKeyPressed))
        let handler: EventHandlerUPP = { _, _, context in
            guard let context else { return OSStatus(eventNotHandledErr) }
            Unmanaged<AppDelegate>.fromOpaque(context).takeUnretainedValue().take()
            return noErr
        }
        let installed = InstallEventHandler(GetApplicationEventTarget(), handler, 1, &event, Unmanaged.passUnretained(self).toOpaque(), &hotKeyHandler)
        let registered = installed == noErr ? RegisterEventHotKey(UInt32(kVK_ANSI_M), UInt32(controlKey | optionKey), EventHotKeyID(signature: 0x474D5357, id: 1), GetApplicationEventTarget(), 0, &hotKey) : installed
        if registered != noErr {
            DispatchQueue.main.async { self.show("Shortcut unavailable", "Control + Option + M is already in use or could not be registered. The mouse menu remains available.") }
        }
    }
    private func draw() {
        item.button?.image = NSImage(systemSymbolName: busy ? "arrow.triangle.2.circlepath" : "magicmouse.fill", accessibilityDescription: "GlideMouse Switch")
        item.button?.contentTintColor = connected && !released ? .systemBlue : .secondaryLabelColor
        item.button?.toolTip = busy ? "Switching mouse…" : connected ? "Magic Mouse connected · click to use here · right-click for options" : waitingForInput ? "Magic Mouse assigned to this Mac · move or click the mouse to connect" : "Click to move the Magic Mouse to this Mac"
    }
    @objc private func deviceConnected(_ notification: IOBluetoothUserNotification, device: IOBluetoothDevice) {
        closeIfReleased(device)
    }
    private func closeIfReleased(_ device: IOBluetoothDevice) {
        guard released, targetAddress.count == 12, normalized(device.addressString ?? "") == targetAddress else { return }
        bluetoothQueue.async {
            guard self.released, device.isConnected() else { return }
            _ = device.closeConnection()
        }
    }
    private func updateGuard() {
        guard connectionWatch != nil else { return }
        let eventTypes: [CGEventType] = [.keyDown, .leftMouseDown, .mouseMoved, .scrollWheel]
        let idle = eventTypes.map { CGEventSource.secondsSinceLastEventType(.combinedSessionState, eventType: $0) }.filter { $0.isFinite && $0 >= 0 }.min() ?? 86400
        let report: [String: Any] = ["active": true, "time": Date().timeIntervalSince1970, "released": released, "localInputIdleSeconds": min(idle, 86400), "shortcutRegistered": hotKey != nil, "shortcut": "Control+Option+M", "hidReleaseReady": configuredDevice()?.responds(to: NSSelectorFromString("forceRemove")) == true]
        if let data = try? JSONSerialization.data(withJSONObject: report) { try? data.write(to: folder.appendingPathComponent("guard-ready.json"), options: .atomic) }
        pollReleaseRequest()
        if !FileManager.default.fileExists(atPath: folder.appendingPathComponent("released").path) {
            bluetoothQueue.async {
                if FileManager.default.fileExists(atPath: self.folder.appendingPathComponent("hid-ignore-active").path) {
                    self.restoreSystemHID()
                }
            }
        }
        if released {
            let address = targetAddress
            if address.count == 12 {
                let formatted = stride(from: 0, to: 12, by: 2).map { i -> String in let a = address.index(address.startIndex, offsetBy: i); return String(address[a..<address.index(a, offsetBy: 2)]) }.joined(separator: "-")
                bluetoothQueue.async { if self.released, let device = IOBluetoothDevice(addressString: formatted) { _ = device.closeConnection() } }
            }
        }
    }
    private func configuredDevice() -> IOBluetoothDevice? {
        let address = targetAddress
        guard address.count == 12 else { return nil }
        let formatted = stride(from: 0, to: 12, by: 2).map { i -> String in let a = address.index(address.startIndex, offsetBy: i); return String(address[a..<address.index(a, offsetBy: 2)]) }.joined(separator: "-")
        return IOBluetoothDevice(addressString: formatted)
    }
    private func restoreSystemHID() {
        guard let device = configuredDevice() else { return }
        IOBluetoothRemoveIgnoredHIDDevice(unsafeBitCast(device, to: IOBluetoothDeviceRef.self))
        try? FileManager.default.removeItem(at: folder.appendingPathComponent("hid-ignore-active"))
    }
    private func pollReleaseRequest() {
        guard let data = try? Data(contentsOf: folder.appendingPathComponent("hid-release-request.json")),
              let request = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let token = request["Token"] as? String, UUID(uuidString: token) != nil,
              token != lastReleaseRequest,
              let address = request["Address"] as? String, normalized(address) == targetAddress,
              let expires = request["Expires"] as? Double,
              expires > Date().timeIntervalSince1970, expires < Date().timeIntervalSince1970 + 15 else { return }
        lastReleaseRequest = token
        bluetoothQueue.async {
            let result = self.removeConfiguredDevice(expires: expires)
            let response: [String: Any] = ["Token": token, "Completed": result.completed, "Mode": "system-device-removal", "Message": result.detail]
            if let bytes = try? JSONSerialization.data(withJSONObject: response) {
                try? bytes.write(to: self.folder.appendingPathComponent("hid-release-response.json"), options: .atomic)
            }
        }
    }
    private func removeConfiguredDevice(expires: Double) -> (completed: Bool, detail: String) {
        let selector = NSSelectorFromString("forceRemove")
        guard expires > Date().timeIntervalSince1970, let device = configuredDevice(),
              let method = class_getInstanceMethod(type(of: device), selector),
              let encoding = method_getTypeEncoding(method), String(cString: encoding) == "v16@0:8" else {
            return (false, "The Mac system cleanup operation is unavailable")
        }
        typealias CleanDisconnect = @convention(c) (AnyObject, Selector) -> Void
        let clean = unsafeBitCast(method_getImplementation(method), to: CleanDisconnect.self)
        clean(device, selector)
        for _ in 0..<50 {
            if (!device.isConnected() && !device.isPaired()) || Date().timeIntervalSince1970 >= expires { break }
            Thread.sleep(forTimeInterval: 0.1)
        }
        let completed = !device.isConnected() && !device.isPaired()
        return (completed, completed ? "Mac device manager completed mouse removal" : "The Mac system cleanup did not close the mouse link")
    }
    @objc private func clicked() {
        if NSApp.currentEvent?.type == .rightMouseUp {
            let menu = NSMenu()
            let useHere = menu.addItem(withTitle: "Use Magic Mouse Here", action: #selector(take), keyEquivalent: "m")
            useHere.keyEquivalentModifierMask = [.control, .option]
            menu.addItem(withTitle: "Release Magic Mouse", action: #selector(releaseMouse), keyEquivalent: "")
            menu.addItem(.separator())
            menu.addItem(withTitle: "Quit GlideMouse Switch", action: #selector(quit), keyEquivalent: "q")
            for entry in menu.items { entry.target = self }
            item.menu = menu; item.button?.performClick(nil); item.menu = nil
        } else { take() }
    }
    @objc private func take() { action("take") }
    @objc private func releaseMouse() { action("release") }
    @objc private func quit() { manualQuit = true; NSApp.terminate(nil) }
    private func action(_ command: String) {
        guard !busy else { return }
        dismissError()
        busy = true; draw()
        if command == "take" {
            // Drain previously queued release work before starting a claim;
            // an old close must never race the newly established connection.
            bluetoothQueue.async {
                DispatchQueue.main.async { self.performAction(command) }
            }
        } else { performAction(command) }
    }
    private func performAction(_ command: String) {
        run(command) { report in
            self.busy = false
            self.connected = report?["Connected"] as? Bool ?? self.connected
            self.waitingForInput = report?["WaitingForInput"] as? Bool ?? false
            self.draw()
            if report?["Ok"] as? Bool != true {
                self.show("Mouse switch did not finish", report?["Message"] as? String ?? "The helper did not respond. Try opening GlideMouse Switch again.")
            }
        }
    }
    private func refresh() {
        guard !busy, !refreshing else { return }
        refreshing = true
        run("status") { report in
            self.refreshing = false
            // A temporary status timeout must not flash the icon to disconnected.
            if report?["Ok"] as? Bool == true { self.connected = report?["Connected"] as? Bool ?? false; self.waitingForInput = report?["WaitingForInput"] as? Bool ?? false; self.draw() }
        }
    }
    private func run(_ command: String, completion: @escaping ([String: Any]?) -> Void) {
        let executable = runtime
        DispatchQueue.global(qos: .userInitiated).async {
            let child = Process(); let output = Pipe()
            child.executableURL = executable; child.arguments = [command]
            child.standardOutput = output; child.standardError = FileHandle.nullDevice
            var result: [String: Any]?
            do {
                try child.run()
                let data = output.fileHandleForReading.readDataToEndOfFile()
                child.waitUntilExit()
                result = try JSONSerialization.jsonObject(with: data) as? [String: Any]
            } catch { result = ["Ok": false, "Message": error.localizedDescription] }
            let snapshot = result
            DispatchQueue.main.async { completion(snapshot) }
        }
    }
    private func show(_ title: String, _ message: String) {
        NSApp.activate(ignoringOtherApps: true)
        let alert = NSAlert(); alert.messageText = title; alert.informativeText = message
        activeAlert?.window.close()
        alert.addButton(withTitle: "OK")
        alert.buttons.first?.target = self
        alert.buttons.first?.action = #selector(dismissError)
        activeAlert = alert
        alert.window.makeKeyAndOrderFront(nil)
    }
    @objc private func dismissError() { activeAlert?.window.close(); activeAlert = nil }
}
let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.accessory)
app.run()
