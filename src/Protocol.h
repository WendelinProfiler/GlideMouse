#pragma once
#include <stdint.h>
#define IMT_VERSION 1u
#define IMT_MAX_REPORT 142u
#define IMT_RING_SIZE 128u
#define IMT_BATCH_SIZE 16u
#define IMT_DEVICE_TYPE 0x8000u
#define IMT_IOCTL_READ ((IMT_DEVICE_TYPE<<16)|(1u<<14)|(0x801u<<2))
#define IMT_IOCTL_LEASE ((IMT_DEVICE_TYPE<<16)|(2u<<14)|(0x802u<<2))
#define IMT_IOCTL_STATUS ((IMT_DEVICE_TYPE<<16)|(1u<<14)|(0x803u<<2))
typedef struct IMT_RECORD {uint64_t sequence;int64_t qpc;uint32_t length;uint32_t reserved;uint8_t bytes[IMT_MAX_REPORT];uint8_t padding[2];} IMT_RECORD;
typedef struct IMT_READ {uint32_t version;uint32_t reserved;uint64_t afterSequence;} IMT_READ;
typedef struct IMT_BATCH {uint32_t version;uint32_t count;uint64_t missed;IMT_RECORD records[IMT_BATCH_SIZE];} IMT_BATCH;
typedef struct IMT_LEASE {uint32_t version;uint32_t milliseconds;} IMT_LEASE;
typedef struct IMT_STATUS {uint32_t version;uint32_t devices;uint64_t frames;uint64_t malformed;int64_t qpcFrequency;uint32_t leaseActive;uint32_t reserved;} IMT_STATUS;
