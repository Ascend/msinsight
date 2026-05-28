/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_SOURCETEST_H
#define PROFILER_SERVER_SOURCETEST_H

#include <string>

const std::string FOREACH_TILLING_DEF_H_PATH = "/home/test/workspace/foreach/common/foreach/foreach_tiling_def.h";
const std::string FOREACH_TILLING_DEF_H = R"(
#ifndef __FOREACH_TILING_DEF_H__
#define __FOREACH_TILING_DEF_H__

#include <cstdint>
#include <cstring>

#include "kernel_tiling/kernel_tiling.h"

#ifdef __CCE_KT_TEST__
#define __aicore__
#else
#define __aicore__ [aicore]
#endif

inline __aicore__ int32_t AlignDiv32(int32_t n)
{
    return ((n + 31) & ~31) / 32;
}

DTYPE_SCALAR float

constexpr uint16_t MAX_TENSOR_CONT = 256;
constexpr uint16_t MAX_CORE_CONT = 64;

#pragma pack(1)
struct ForeachCommonTilingData {
    uint64_t inputsTensorUbSize = 0;
    int64_t tensorDataCountList[256] = {};
    uint16_t tensorStartList[64] = {};
    uint16_t tensorEndList[64] = {};
    int64_t tensorStartOffsetList[64] = {};
    int64_t tensorEndOffsetList[64] = {};
};
#pragma pack()

COPY_ARR(arrA, arrB, count)        \
    for (uint16_t i = 0; i < count; i++) { \
        arrA[i] = arrB[i];                 \
    }

CONVERT_TILING_DATA(tilingStruct, tilingDataPointer, tilingPointer) \
    __ubuf__ tilingStruct* tilingDataPointer =                              \
        reinterpret_cast<__ubuf__ tilingStruct*>((__ubuf__ uint8_t*)(tilingPointer));

#ifdef __CCE_KT_TEST__
INIT_TILING_DATA(tilingStruct, tilingDataPointer, tilingPointer) \
    CONVERT_TILING_DATA(tilingStruct, tilingDataPointer, tilingPointer);
#else
INIT_TILING_DATA(tilingStruct, tilingDataPointer, tilingPointer)                        \
    __ubuf__ uint8_t* tilingUbPointer = (__ubuf__ uint8_t*)get_imm(0);                          \
    copy_gm_to_ubuf(((__ubuf__ uint8_t*)(tilingUbPointer)), ((__gm__ uint8_t*)(tilingPointer)), \
        0, 1, AlignDiv32(sizeof(tilingStruct)), 0, 0);                                          \
    CONVERT_TILING_DATA(tilingStruct, tilingDataPointer, tilingUbPointer);                      \
    pipe_barrier(PIPE_ALL);
#endif

GET_TILING_DATA(tilingData, tilingPointer)                                                        \
    ForeachCommonTilingData tilingData;                                                 \
    INIT_TILING_DATA(ForeachCommonTilingData, tilingDataPointer, tilingPointer);        \
    (tilingData).inputsTensorUbSize = tilingDataPointer->inputsTensorUbSize;                                \
    COPY_ARR((tilingData).tensorDataCountList, tilingDataPointer->tensorDataCountList, MAX_TENSOR_CONT)   \
    COPY_ARR((tilingData).tensorStartList, tilingDataPointer->tensorStartList, MAX_CORE_CONT)             \
    COPY_ARR((tilingData).tensorEndList, tilingDataPointer->tensorEndList, MAX_CORE_CONT)                 \
    COPY_ARR((tilingData).tensorStartOffsetList, tilingDataPointer->tensorStartOffsetList, MAX_CORE_CONT) \
    COPY_ARR((tilingData).tensorEndOffsetList, tilingDataPointer->tensorEndOffsetList, MAX_CORE_CONT)
#endif
)";

const std::string KERNEL_FOREACH_BASE_H_PATH =
    "/home/test/workspace/foreach/common/foreach/op_kernel/kernel_foreach_base.h";
const std::string KERNEL_FOREACH_BASE_H = R"(
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#ifndef KERNEL_FOREACH_BASE_H
#define KERNEL_FOREACH_BASE_H

#include <type_traits>
#include "kernel_operator.h"

namespace Common {
namespace OpKernel {
using namespace AscendC;

constexpr uint8_t COPY_SPACE_MULTIPLE = 9;

template <typename T>
class KernelForeachBase {
protected:
    __aicore__ inline KernelForeachBase() {};

    __aicore__ inline void Init(ForeachCommonTilingData* tilingData);
    __aicore__ inline void ParseTilingData(ForeachCommonTilingData* tilingData);
    __aicore__ inline __gm__ T* GetTensorAddr(uint16_t index, GM_ADDR tensorPtr);

    template <typename T1, typename T2>
    __aicore__ inline T1 CeilA2B(T1 a, T2 b) {
        if (b == 0) {
            return a;
        }
        return (a + b - 1) / b;
    };

protected:
    TPipe pipe;

    int64_t blockIdx = 0;

    // tiling params
    uint64_t inputsTensorUbSize = 0;
    int64_t* tensorDataCountList = nullptr;
    uint16_t tensorStart = 0;
    uint16_t tensorEnd = 0;
    int64_t tensorStartOffset = 0;
    int64_t tensorEndOffset = 0;

    uint64_t totalTensorUbSize = 0;
    uint32_t maxDataCount = 0;
    uint32_t maxCastDataCount = 0;
};

template <typename T>
__aicore__ inline void KernelForeachBase<T>::Init(
    ForeachCommonTilingData* tilingData) {
    blockIdx = GetBlockIdx();

    ParseTilingData(tilingData);

    #if __CCE_AICORE__ == 220
        if (std::is_same_v<T, bfloat16_t>) {
            totalTensorUbSize = inputsTensorUbSize * COPY_SPACE_MULTIPLE;
            maxDataCount = totalTensorUbSize / sizeof(T);
            maxCastDataCount = inputsTensorUbSize / sizeof(float);
        } else {
            maxDataCount = inputsTensorUbSize / sizeof(T);
        }
    #else
        maxDataCount = inputsTensorUbSize / sizeof(T);
    #endif
}

template <typename T>
__aicore__ inline void KernelForeachBase<T>::ParseTilingData(
    ForeachCommonTilingData* tilingData) {
    inputsTensorUbSize = tilingData->inputsTensorUbSize;
    tensorDataCountList = tilingData->tensorDataCountList;
    tensorStart = tilingData->tensorStartList[blockIdx];
    tensorEnd = tilingData->tensorEndList[blockIdx];
    tensorStartOffset = tilingData->tensorStartOffsetList[blockIdx];
    tensorEndOffset = tilingData->tensorEndOffsetList[blockIdx];
}

template <typename T>
__aicore__ inline __gm__ T* KernelForeachBase<T>::GetTensorAddr(uint16_t index, GM_ADDR tensorPtr) {
    __gm__ uint64_t* dataAddr = reinterpret_cast<__gm__ uint64_t*>(tensorPtr);
    uint64_t tensorPtrOffset = *dataAddr;  // The offset of the data address from the first address.
    // Moving 3 bits to the right means dividing by sizeof(uint64 t).
    __gm__ uint64_t* retPtr = dataAddr + (tensorPtrOffset >> 3);
    return reinterpret_cast<__gm__ T*>(*(retPtr + index));
}

}  // namespace OpKernel
}  // namespace Common

#endif  // KERNEL_FOREACH_BASE_H
)";

const std::string FOREACH_SUB_SCALAR_LIST_CPP_PATH = "/home/test/workspace/foreach/foreach_sub_scalar_list.cpp";
const std::string FOREACH_SUB_SCALAR_LIST_CPP = R"(
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 * This file contains code of cpu debug and npu code. We read data from bin file
 * and write result to file.
 */

#include "kernel_operator.h"

// op kernel building at build_out directory, it's not fully aligned with source code structure
// current op_kernel folder is absent in build_out directory, so the relative path to common has just one layer
#include "common/foreach/foreach_tiling_def.h"
#include "common/foreach/op_kernel/foreach_one_scalar_list_binary_level_zero_api.h"

using namespace AscendC;
using namespace Common::OpKernel;

extern __global__ __aicore__ void foreach_sub_scalar_list(GM_ADDR x, GM_ADDR scalar, GM_ADDR y,
                                                        GM_ADDR workspace, GM_ADDR tiling) {

GET_TILING_DATA(tilingData, tiling);

//foreach(vector) not need workspace
GM_ADDR userWS = nullptr;

ForeachOneScalarListBinaryLevelZeroApi<float, float, Sub, 2, 1> op;
op.Init(x, scalar, y, userWS, &tilingData);
op.Process();

if (TILING_KEY_IS(1)) {
    ForeachOneScalarListBinaryLevelZeroApi<half, half, Sub, 2, 1> op;
    op.Init(x, scalar, y, userWS, &tilingData);
    op.Process();
} else if (TILING_KEY_IS(2)) {
    ForeachOneScalarListBinaryLevelZeroApi<float, float, Sub, 2, 1> op;
    op.Init(x, scalar, y, userWS, &tilingData);
    op.Process();
} else if (TILING_KEY_IS(3)) {
    ForeachOneScalarListBinaryLevelZeroApi<int, int, Sub, 2, 1> op;
    op.Init(x, scalar, y, userWS, &tilingData);
    op.Process();
}
#if __CCE_AICORE__ == 220
else if (TILING_KEY_IS(4)) {
    ForeachOneScalarListBinaryLevelZeroApi<bfloat16_t, float, Sub, 2, 1> op;
    op.Init(x, scalar, y, userWS, &tilingData);
    op.Process();
}
#endif
}
)";

const std::string API_INSTR_JSON = R"(
{
	"Cores" : ["core0.veccore0", "core0.veccore1", "core1.veccore0"],
	"Instructions" : [{
			"Address" : "0x1124400c",
			"AscendC Inner Code" : "/home/test/workspace/foreach/foreach_sub_scalar_list.cpp:18",
			"Cycles" : [1.0, 1.0, 1.0],
			"Instructions Executed" : [1, 1, 1],
			"Pipe" : "SCALAR",
			"Source" : "ADD XD:X16=0x100000,XD:X16:0x100000,XM:X15=0,XM:X15:0,XN:X16=0x100000,XN:X16:0x100000,dtype:S64"
		}, {
			"Address" : "0x11244010",
			"AscendC Inner Code" : "/home/test/workspace/foreach/foreach_sub_scalar_list.cpp:18",
			"Cycles" : [1.0, 1.0, 1.0],
			"Instructions Executed" : [1, 1, 1],
			"Pipe" : "SCALAR",
			"Source" : "MOV_XD_SPR SPR:COREID,XD:X15=0x18,XD:X15:0x18"
		}, {
			"Address" : "0x11244014",
			"AscendC Inner Code" : "/home/test/workspace/foreach/foreach_sub_scalar_list.cpp:18",
			"Cycles" : [1.0, 1.0, 1.0],
			"Instructions Executed" : [1, 1, 1],
			"Pipe" : "SCALAR",
			"Source" : "MOV_XD_IMM IMM:0x7fff,XD:X17=0x7fff,XD:X17:0x7fff"
		}, {
			"Address" : "0x11244018",
			"AscendC Inner Code" : "/home/test/workspace/foreach/foreach_sub_scalar_list.cpp:18",
			"Cycles" : [1.0, 1.0, 1.0],
			"Instructions Executed" : [1, 1, 1],
			"Pipe" : "SCALAR",
			"Source" : "AND XD:X15=0x18,XD:X15:0x18,XM:X17=0x7fff,XM:X17:0x7fff,XN:X15=0x18,XN:X15:0x18,dtype:B64"
		}, {
			"Address" : "0x1124401c",
			"AscendC Inner Code" : "/home/test/workspace/foreach/foreach_sub_scalar_list.cpp:18",
			"Cycles" : [1.0, 1.0, 1.0],
			"Instructions Executed" : [1, 1, 1],
			"Pipe" : "SCALAR",
			"Source" : "MOV_XD_IMM IMM:0x4000,XD:X17=0x4000,XD:X17:0x4000"
		}, {
			"Address" : "0x11244020",
			"AscendC Inner Code" : "/home/test/workspace/foreach/foreach_sub_scalar_list.cpp:18",
			"Cycles" : [1.0, 1.0, 1.0],
			"Instructions Executed" : [1, 1, 1],
			"Pipe" : "SCALAR",
			"Source" : "MOV_XD_IMM IMM:0x1,XD:X0=0x1,XD:X0:0x1"
		}, {
			"Address" : "0x11244024",
			"AscendC Inner Code" : "/home/test/workspace/foreach/foreach_sub_scalar_list.cpp:18",
			"Cycles" : [3.0, 3.0, 3.0],
			"Instructions Executed" : [1, 1, 1],
			"Pipe" : "SCALAR",
			"Source" : "MADD XD:X16=0x160000,XD:X16:0x160000,XM:X17=0x4000,XM:X17:0x4000,XN:X15=0x18,XN:X15:0x18,dtype:S64"
		}, {
			"Address" : "0x11244808",
			"AscendC Inner Code" : "/home/test/workspace/foreach/common/foreach/foreach_tiling_def.h:32",
			"Cycles" : [37.0, 37.0, 37.0],
			"Instructions Executed" : [1, 1, 1],
			"Pipe" : "SCALAR",
			"Source" : "STI_XN_IMM #IMM_TYPE:ZERO,#POST:0,IMM:0,XN:X8=0x163f78,XN:X8:0x163f78,dtype:B64"
		}, {
			"Address" : "0x1124482c",
			"AscendC Inner Code" : "/home/test/workspace/foreach/common/foreach/foreach_tiling_def.h:32",
			"Cycles" : [1.0, 1.0, 1.0],
			"Instructions Executed" : [1, 1, 1],
			"Pipe" : "SCALAR",
			"Source" : "ADD XD:X7=0x163f90,XD:X7:0x163f90,XM:X7=0xd10,XM:X7:0xd10,XN:X30=0x163280,XN:X30:0x163280,dtype:S64"
		}, {
			"Address" : "0x11244830",
			"AscendC Inner Code" : "/home/test/workspace/foreach/common/foreach/foreach_tiling_def.h:32",
			"Cycles" : [1.0, 1.0, 1.0],
			"Instructions Executed" : [1, 1, 1],
			"Pipe" : "SCALAR",
			"Source" : "ADD XD:X8=0x163f98,XD:X8:0x163f98,XM:X8=0xd18,XM:X8:0xd18,XN:X30=0x163280,XN:X30:0x163280,dtype:S64"
		}, {
			"Address" : "0x11244834",
			"AscendC Inner Code" : "/home/test/workspace/foreach/common/foreach/foreach_tiling_def.h:32",
			"Cycles" : [299.0, 300.0, 296.0],
			"Instructions Executed" : [1, 1, 1],
			"Pipe" : "SCALAR",
			"Source" : "STI_XN_IMM #IMM_TYPE:ZERO,#POST:0,IMM:0,XN:X7=0x163f90,XN:X7:0x163f90,dtype:B64"
		}, {
			"Address" : "0x11244838",
			"AscendC Inner Code" : "/home/test/workspace/foreach/common/foreach/foreach_tiling_def.h:32",
			"Cycles" : [299.0, 300.0, 296.0],
			"Instructions Executed" : [1, 1, 1],
			"Pipe" : "SCALAR",
			"Source" : "STI_XN_IMM #IMM_TYPE:ZERO,#POST:0,IMM:0,XN:X8=0x163f98,XN:X8:0x163f98,dtype:B64"
		}
	]
}
)";

const std::string API_FILE_JSON = R"(
{
	"Cores" : ["core0.veccore0", "core0.veccore1", "core1.veccore0"],
	"Files" : [{
			"Lines" : [{
					"Address Range" : [["0x11244240", "0x11244240"], ["0x11244248", "0x1124453c"]],
					"Cycles" : [15674.0, 15715.0, 15707.0],
					"Instructions Executed" : [192, 192, 192],
					"Line" : 31
				}, {
					"Address Range" : [["0x11244540", "0x11244540"], ["0x11244548", "0x11244844"]],
					"Cycles" : [16293.0, 16335.0, 16309.0],
					"Instructions Executed" : [193, 193, 193],
					"Line" : 32
				}
			],
			"Source" : "/home/test/workspace/foreach/common/foreach/foreach_tiling_def.h"
		}, {
			"Lines" : [{
					"Address Range" : [["0x11244df0", "0x11244df0"], ["0x11244e38", "0x11244e38"]],
					"Cycles" : [729.0, 727.0, 731.0],
					"Instructions Executed" : [2, 2, 2],
					"Line" : 85
				}, {
					"Address Range" : [["0x11244e0c", "0x11244e10"], ["0x11244e40", "0x11244e44"]],
					"Cycles" : [4.0, 4.0, 4.0],
					"Instructions Executed" : [4, 4, 4],
					"Line" : 87
				}, {
					"Address Range" : [["0x11244e18", "0x11244e18"], ["0x11244e48", "0x11244e48"]],
					"Cycles" : [727.0, 727.0, 729.0],
					"Instructions Executed" : [2, 2, 2],
					"Line" : 88
				}
			],
			"Source" : "/home/test/workspace/foreach/common/foreach/op_kernel/kernel_foreach_base.h"
		}, {
			"Lines" : [{
					"Address Range" : [["0x11244000", "0x11244060"]],
					"Cycles" : [59.0, 59.0, 59.0],
					"Instructions Executed" : [25, 25, 25],
					"Line" : 18
				}, {
					"Address Range" : [["0x11244064", "0x11244848"], ["0x1124485c", "0x11244898"]],
					"Cycles" : [264407.0, 264702.0, 264186.0],
					"Instructions Executed" : [7489, 7489, 7489],
					"Line" : 20
				}, {
					"Address Range" : [["0x11244954", "0x11244a90"], ["0x11244ca4", "0x11244ca8"]],
					"Cycles" : [15703.5, 15723.5, 15738.5],
					"Instructions Executed" : [82, 82, 82],
					"Line" : 25
				}, {
					"Address Range" : [["0x11244a94", "0x11244c08"], ["0x11244c10", "0x11244c94"]],
					"Cycles" : [7767.0, 7496.0, 7583.0],
					"Instructions Executed" : [128, 128, 128],
					"Line" : 26
				}
			],
			"Source" : "/home/test/workspace/foreach/foreach_sub_scalar_list.cpp"
		}
	]
}
)";
#endif // PROFILER_SERVER_SOURCETEST_H
