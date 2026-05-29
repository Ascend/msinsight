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
#include "AdvisorPlugin.h"
#include "GlobalPlugin.h"
#include "MemoryPlugin.h"
#include "OperatorPlugin.h"
#include "SourcePlugin.h"
#include "SummaryPlugin.h"
#include "TimelinePlugin.h"
#include "CommunicationPlugin.h"
#include "IEPlugin.h"
#include "MemScopePlugin.h"
#include "RLPlugin.h"
#include "TritonPlugin.h"
namespace Dic::Module {
Core::PluginRegister ADVISOR_PLUGIN(std::make_unique<Advisor::AdvisorPlugin>());
Core::PluginRegister GLOBAL_PLUGIN(std::make_unique<Global::GlobalPlugin>());
Core::PluginRegister MEMORY_PLUGIN(std::make_unique<Memory::MemoryPlugin>());
Core::PluginRegister OPERATOR_PLUGIN(std::make_unique<Operator::OperatorPlugin>());
Core::PluginRegister SOURCE_PLUGIN(std::make_unique<Source::SourcePlugin>());
Core::PluginRegister SUMMARY_PLUGIN(std::make_unique<Summary::SummaryPlugin>());
Core::PluginRegister TIMELINE_PLUGIN(std::make_unique<Timeline::TimelinePlugin>());
Core::PluginRegister COMM_PLUGIN(std::make_unique<Communication::CommunicationPlugin>());
Core::PluginRegister IE_PLUGIN(std::make_unique<IE::IEPlugin>());
Core::PluginRegister MEM_SCOPE_PLUGIN(std::make_unique<MemScope::MemScopePlugin>());
Core::PluginRegister RL_PLUGIN(std::make_unique<RL::RLPlugin>());
Core::PluginRegister TRITON_PLUGIN(std::make_unique<Triton::TritonPlugin>());
}
