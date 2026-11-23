#pragma once
#include "MagicMouseEngine.h"
namespace DirectMousePointer {
struct Delta {int x=0,y=0;unsigned down=0,up=0;bool Any()const{return x||y||down||up;}};
class State {
 unsigned buttons=0;
public:
 Delta Feed(const MagicMouse::Frame& frame){unsigned next=frame.buttons&3;Delta result{frame.pointerX,frame.pointerY,next&~buttons,buttons&~next};buttons=next;return result;}
 Delta Release(){Delta result{0,0,0,buttons};buttons=0;return result;}
};
}
