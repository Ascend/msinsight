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

#ifndef PROFILER_SERVER_COMPUTESOURCEFILE_H
#define PROFILER_SERVER_COMPUTESOURCEFILE_H

#include <string>

namespace Dic::Module::Source::Test {
const std::string_view SOURCE_NAME = "/test/vec_add1_simt.cpp";
const std::string_view CORE_NAME = "core0.veccore0";
const std::string_view BIN_FILE_PATH_API_LINE = "query_api_line_handler_test.bin";
const std::string_view BIN_FILE_PATH_API_LINE_DYNAMIC = "query_api_line_dynamic_handler_test.bin";
const std::string_view API_FILE_WITH_DTYPE = R"({
"Cores" : ["core0.veccore0", "core0.veccore1"],
"Files Dtype": {
    "Lines": {
        "Address Range": 0,
        "Cycles": 1,
        "Instructions Executed": 1,
        "Line": 1
    }
},
"Files" : [{
        "Lines" : [{
                "Address Range" : [["0x1134e2d8", "0x1134e4d8"], ["0x1134e138", "0x1134e138"]],
                "Cycles" : [56, 56],
                "Instructions Executed" : [8, 8],
                "Line" : 31
            }, {
                "Address Range" : [["0x1134e0f0", "0x1134e0f0"], ["0x1134e0f8", "0x1134e0f8"]],
                "Cycles" : [284, 284],
                "Instructions Executed" : [36, 36],
                "Line" : 32
            }, {
                "Address Range" : [["0x1134e158", "0x1134e158"], ["0x1134e160", "0x1134e160"]],
                "Cycles" : [7729, 1984],
                "Instructions Executed" : [208, 208],
                "Line" : 33
            }, {
                "Address Range" : [["0x1134e0e8", "0x1134e0e8"], ["0x1134e0f0", "0x1134e0f0"]],
                "Cycles" : [8145, 2400],
                "Instructions Executed" : [260, 260],
                "Line" : 41
            }, {
                "Address Range" : [["0x1134e048", "0x1134e0b0"], ["0x1134e0b8", "0x1134e0b8"]],
                "Cycles" : [2670, 1609],
                "Instructions Executed" : [33, 33],
                "Line" : 42
            }, {
                "Address Range" : [["0x1134e000", "0x1134e044"]],
                "Cycles" : [1694, 1442],
                "Instructions Executed" : [18, 18],
                "Line" : 43
            }, {
                "Address Range" : [["0x1134e048", "0x1134e0b0"], ["0x1134e0b8", "0x1134e0b8"]],
                "Cycles" : [2670, 1609],
                "Instructions Executed" : [33, 33],
                "Line" : 46
            }, {
                "Address Range" : [["0x1134e0e0", "0x1134e0e0"]],
                "Cycles" : [0, 0],
                "Instructions Executed" : [1, 1],
                "Line" : 56
            }
        ],
        "Source" : "/test/vec_add1_simt.cpp"
    }
]
})";
const std::string_view INSTR_FILE_WITH_DTYPE = R"({
	"Cores" : ["core0.veccore0", "core0.veccore1"],
    "Instructions Dtype": {
        "Instructions": {
            "Address": 3,
            "AscendC Inner Code": 3,
            "Cycles": 1,
            "Instructions Executed": 1,
            "Pipe": 3,
            "TheoreticalStallCycles": 1,
            "Source": 3,
            "RealStallCycles": 1
         }
    },
	"Instructions" : [{
			"Address" : "0x1134e2d8",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [62, 42],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECLD",
			"RealStallCycles" : [13, 22],
			"Source" : "SIMT_LDG [PEX:6|P],[Rn:0|R],[Rn1:1|R],[Rd:0|R],[#ofst:9],[cop:1],[l2_cache_hint:0]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e2d0",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [360, 44],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECST",
			"RealStallCycles" : [36, 42],
			"Source" : "SIMT_STG [PEX:6|P],[Rn:6|R],[Rn1:7|R],[Rs:3|R],[#ofst:8],[btype:2],[cop:1],[l2_cache_hint:0]",
			"TheoreticalStallCycles" : [28, 28]
		}, {
			"Address" : "0x1134e2c8",
			"AscendC Inner Code" : "/test/compiler/tikcpp/tikcfw/interface/kernel_operator_simt_float_intrinsics.h:104",
			"Cycles" : [32, 32],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECEX",
			"RealStallCycles" : [75, 55],
			"Source" : "SIMT_IADD [PEX:6|P],[Rm:3|R],[Rn:8|R],[Rd:3|R],[waitBitMask:3],[stallCyc:7],[yeild:0]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e2c0",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [63, 43],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECLD",
			"RealStallCycles" : [8, 10],
			"Source" : "SIMT_LDG [PEX:6|P],[Rn:4|R],[Rn1:5|R],[Rd:8|R],[#ofst:8],[cop:1],[l2_cache_hint:0],[rscb_id:7]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e2f8",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:59",
			"Cycles" : [36, 36],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECLP",
			"RealStallCycles" : [16, 16],
			"Source" : "SIMT_END [PEX:7|P],[waitBitMask:3],[stallCyc:a],[yeild:0],[inv:0],[warpId:0],[schId:0]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e2b8",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [62, 41],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECLD",
			"RealStallCycles" : [21, 25],
			"Source" : "SIMT_LDG [PEX:6|P],[Rn:0|R],[Rn1:1|R],[Rd:3|R],[#ofst:8],[cop:1],[l2_cache_hint:0]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e2a0",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [63, 43],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECLD",
			"RealStallCycles" : [8, 10],
			"Source" : "SIMT_LDG [PEX:6|P],[Rn:4|R],[Rn1:5|R],[Rd:8|R],[#ofst:7],[cop:1],[l2_cache_hint:0],[rscb_id:7]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e298",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [62, 41],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECLD",
			"RealStallCycles" : [21, 25],
			"Source" : "SIMT_LDG [PEX:6|P],[Rn:0|R],[Rn1:1|R],[Rd:3|R],[#ofst:7],[cop:1],[l2_cache_hint:0],[rscb_id:7]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e290",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [452, 44],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECST",
			"RealStallCycles" : [36, 42],
			"Source" : "SIMT_STG [PEX:6|P],[Rn:6|R],[Rn1:7|R],[Rs:3|R],[#ofst:6],[btype:2],[cop:1],[l2_cache_hint:0]",
			"TheoreticalStallCycles" : [28, 28]
		}, {
			"Address" : "0x1134e288",
			"AscendC Inner Code" : "/test/compiler/tikcpp/tikcfw/interface/kernel_operator_simt_float_intrinsics.h:104",
			"Cycles" : [32, 32],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECEX",
			"RealStallCycles" : [75, 55],
			"Source" : "SIMT_IADD [PEX:6|P],[Rm:3|R],[Rn:8|R],[Rd:3|R],[waitBitMask:3],[stallCyc:7],[yeild:0],[inv:0]",
			"TheoreticalStallCycles" : [8, 8]
		}
	]
})";
const std::string_view SOURCE_FILE = R"(/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include "kernel_operator_simt.h"
#include "kernel_operator_simt_float_intrinsics.h"
using namespace AscendC;

namespace simt_add {
#define THREAD_DIM 1024

template <typename T>
class KernelAdd {
    public:
        __aicore__ KernelAdd() {}

        public: __aicore__ inline void Init(GM_ADDR out, GM_ADDR src0, GM_ADDR src1, const int size);
        __simt_callee__ inline __aicore__ void SimtCompute() const;
        __aicore__ inline void Process();

    private:
        AscendC::GlobalTensor<T> outGm;
        AscendC::GlobalTensor<T> src0Gm;
        AscendC::GlobalTensor<T> src1Gm;
        int size;
};

template <typename T>
__aicore__ inline void KernelAdd<T>::Init(GM_ADDR out, GM_ADDR src0, GM_ADDR src1,
                    const int size)
{
    outGm.SetGlobalBuffer((__gm__ T*)(out));
    src0Gm.SetGlobalBuffer((__gm__ T*)(src0));
    src1Gm.SetGlobalBuffer((__gm__ T*)(src1));
    this->size = size;
}

template <typename T>
__simt_callee__ inline __aicore__ void KernelAdd<T>::SimtCompute() const
{
    // simt 代码
    auto dst = outGm.address_;
    auto src0 = src0Gm.address_;
    auto src1 = src1Gm.address_;
    for (int idx = block_idx * 32768; idx < GetThreadIdx<0>() * 8192 + block_idx * 32768 + 10; idx++)
    {
        dst[idx] = Add(src0[idx], src1[idx]);
    }
}

template <typename T>
__aicore__ inline  void KernelAdd<T>::Process()
{
        // 使用lambda 封装simt_vf simt_vf函数SimtCompute
        auto simt_func = [=, *this]() { SimtCompute(); };
        // 启动SIMT_VF
        ParallelEXE({128}, simt_func);
}

extern "C" __global__ __aicore__ void vec_add1(GM_ADDR src0, GM_ADDR src1, GM_ADDR out, GM_ADDR gm_tiling)
{
    simt_add::KernelAdd<DType> op;
    op.Init(out, src0, src1, 32768);
    op.Process();
}
})";

const std::string_view API_FILE = R"({
"Cores" : ["core0.veccore0", "core0.veccore1"],
"Files" : [{
        "Lines" : [{
                "Address Range" : [["0x1134e2d8", "0x1134e4d8"], ["0x1134e138", "0x1134e138"]],
                "Cycles" : [56, 56],
                "Instructions Executed" : [8, 8],
                "Line" : 31
            }, {
                "Address Range" : [["0x1134e0f0", "0x1134e0f0"], ["0x1134e0f8", "0x1134e0f8"]],
                "Cycles" : [284, 284],
                "Instructions Executed" : [36, 36],
                "Line" : 32
            }, {
                "Address Range" : [["0x1134e158", "0x1134e158"], ["0x1134e160", "0x1134e160"]],
                "Cycles" : [7729, 1984],
                "Instructions Executed" : [208, 208],
                "Line" : 33
            }, {
                "Address Range" : [["0x1134e0e8", "0x1134e0e8"], ["0x1134e0f0", "0x1134e0f0"]],
                "Cycles" : [8145, 2400],
                "Instructions Executed" : [260, 260],
                "Line" : 41
            }, {
                "Address Range" : [["0x1134e048", "0x1134e0b0"], ["0x1134e0b8", "0x1134e0b8"]],
                "Cycles" : [2670, 1609],
                "Instructions Executed" : [33, 33],
                "Line" : 42
            }, {
                "Address Range" : [["0x1134e000", "0x1134e044"]],
                "Cycles" : [1694, 1442],
                "Instructions Executed" : [18, 18],
                "Line" : 43
            }, {
                "Address Range" : [["0x1134e048", "0x1134e0b0"], ["0x1134e0b8", "0x1134e0b8"]],
                "Cycles" : [2670, 1609],
                "Instructions Executed" : [33, 33],
                "Line" : 46
            }, {
                "Address Range" : [["0x1134e0e0", "0x1134e0e0"]],
                "Cycles" : [0, 0],
                "Instructions Executed" : [1, 1],
                "Line" : 56
            }
        ],
        "Source" : "/test/vec_add1_simt.cpp"
    }
]
})";
const std::string_view INSTR_FILE = R"({
	"Cores" : ["core0.veccore0", "core0.veccore1"],
	"Instructions" : [{
			"Address" : "0x1134e2d8",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [62, 42],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECLD",
			"RealStallCycles" : [13, 22],
			"Source" : "SIMT_LDG [PEX:6|P],[Rn:0|R],[Rn1:1|R],[Rd:0|R],[#ofst:9],[cop:1],[l2_cache_hint:0]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e2d0",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [360, 44],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECST",
			"RealStallCycles" : [36, 42],
			"Source" : "SIMT_STG [PEX:6|P],[Rn:6|R],[Rn1:7|R],[Rs:3|R],[#ofst:8],[btype:2],[cop:1],[l2_cache_hint:0]",
			"TheoreticalStallCycles" : [28, 28]
		}, {
			"Address" : "0x1134e2c8",
			"AscendC Inner Code" : "/test/compiler/tikcpp/tikcfw/interface/kernel_operator_simt_float_intrinsics.h:104",
			"Cycles" : [32, 32],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECEX",
			"RealStallCycles" : [75, 55],
			"Source" : "SIMT_IADD [PEX:6|P],[Rm:3|R],[Rn:8|R],[Rd:3|R],[waitBitMask:3],[stallCyc:7],[yeild:0]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e2c0",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [63, 43],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECLD",
			"RealStallCycles" : [8, 10],
			"Source" : "SIMT_LDG [PEX:6|P],[Rn:4|R],[Rn1:5|R],[Rd:8|R],[#ofst:8],[cop:1],[l2_cache_hint:0],[rscb_id:7]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e2f8",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:59",
			"Cycles" : [36, 36],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECLP",
			"RealStallCycles" : [16, 16],
			"Source" : "SIMT_END [PEX:7|P],[waitBitMask:3],[stallCyc:a],[yeild:0],[inv:0],[warpId:0],[schId:0]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e2b8",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [62, 41],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECLD",
			"RealStallCycles" : [21, 25],
			"Source" : "SIMT_LDG [PEX:6|P],[Rn:0|R],[Rn1:1|R],[Rd:3|R],[#ofst:8],[cop:1],[l2_cache_hint:0]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e2a0",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [63, 43],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECLD",
			"RealStallCycles" : [8, 10],
			"Source" : "SIMT_LDG [PEX:6|P],[Rn:4|R],[Rn1:5|R],[Rd:8|R],[#ofst:7],[cop:1],[l2_cache_hint:0],[rscb_id:7]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e298",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [62, 41],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECLD",
			"RealStallCycles" : [21, 25],
			"Source" : "SIMT_LDG [PEX:6|P],[Rn:0|R],[Rn1:1|R],[Rd:3|R],[#ofst:7],[cop:1],[l2_cache_hint:0],[rscb_id:7]",
			"TheoreticalStallCycles" : [8, 8]
		}, {
			"Address" : "0x1134e290",
			"AscendC Inner Code" : "/test/vec_add1_simt.cpp:50",
			"Cycles" : [452, 44],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECST",
			"RealStallCycles" : [36, 42],
			"Source" : "SIMT_STG [PEX:6|P],[Rn:6|R],[Rn1:7|R],[Rs:3|R],[#ofst:6],[btype:2],[cop:1],[l2_cache_hint:0]",
			"TheoreticalStallCycles" : [28, 28]
		}, {
			"Address" : "0x1134e288",
			"AscendC Inner Code" : "/test/compiler/tikcpp/tikcfw/interface/kernel_operator_simt_float_intrinsics.h:104",
			"Cycles" : [32, 32],
			"Instructions Executed" : [4, 4],
			"Pipe" : "RVECEX",
			"RealStallCycles" : [75, 55],
			"Source" : "SIMT_IADD [PEX:6|P],[Rm:3|R],[Rn:8|R],[Rd:3|R],[waitBitMask:3],[stallCyc:7],[yeild:0],[inv:0]",
			"TheoreticalStallCycles" : [8, 8]
		}
	]
})";

}
#endif // PROFILER_SERVER_COMPUTESOURCEFILE_H
