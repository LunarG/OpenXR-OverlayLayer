// Copyright (c) 2022 LunarG, Inc.
// Copyright (c) 2022 PlutoVR Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: John Zulauf <jzulauf@lunarg.com>

#pragma once
#ifndef _ADJUSTED_REF_SPACE_H_
#define _ADJUSTED_REF_SPACE_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <map>
#include <memory>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include "xr_generated_dispatch_table.h"

namespace adjusted_reference_space_sublayer {

// Public interface for sublayer
void CreateInstance(XrInstance, XrGeneratedDispatchTable*);

// Per Instance table (as downstream could vary per Instance, based on extensions/layers)
struct DispatchTable {
    DispatchTable();
    void DispatchTable::Patch(XrGeneratedDispatchTable* downchain);

    PFN_xrDestroyInstance DestroyInstance;
    PFN_xrCreateSession CreateSession;
    PFN_xrDestroySession DestroySession;
    PFN_xrCreateReferenceSpace CreateReferenceSpace;
    PFN_xrLocateViews LocateViews;
    PFN_xrLocateSpace LocateSpace;
    PFN_xrDestroySpace DestroySpace;
};

struct SessionInfo;

struct Adjustment {
    XrTime time = 0;
    XrPosef pose = {};
    Adjustment() : time(0), pose() {}
};

struct InstanceInfo {
    InstanceInfo(XrInstance instance_) : instance_handle(instance_), downchain(), sessions() {}
    XrInstance instance_handle;
    DispatchTable downchain;
    std::unordered_map<SessionInfo*, std::weak_ptr<SessionInfo>> sessions;
};
using AdjustmentTimeline = std::map<XrTime, std::shared_ptr<Adjustment>>;

class SpaceAlias {
  public:
    XrSpace Handle() const { return alias_handle_; }
    bool Valid(XrTime limit) const;
    SpaceAlias() = default;
    SpaceAlias(SpaceAlias&&) = default;
    SpaceAlias(const SpaceAlias&) = default;
    SpaceAlias(XrSpace alias, std::shared_ptr<Adjustment>& adjustment) : alias_handle_(alias), adjustment_(adjustment) {}
    SpaceAlias& operator=(SpaceAlias&&) = default;
    SpaceAlias& operator=(const SpaceAlias&) = default;
    SpaceAlias& Set(XrSpace alias, std::shared_ptr<Adjustment>& adjustment);

  private:
    XrSpace alias_handle_ = XR_NULL_HANDLE;
    std::weak_ptr<Adjustment> adjustment_;
};

struct SpaceInfo {
    XrSpace space_handle;
    std::weak_ptr<SessionInfo> session;
    XrReferenceSpaceCreateInfo create_info;
    using AdjustmentAliases = std::unordered_map<const Adjustment*, std::shared_ptr<SpaceAlias>>;
    AdjustmentAliases aliases;
    std::vector<std::unique_ptr<SpaceAlias>> expired_aliases;
    std::shared_ptr<SpaceAlias> FindOrCreateAlias(SessionInfo& session, XrTime time);

    std::shared_ptr<SessionInfo> Session() const { return session.lock(); }
    AdjustmentAliases::iterator AddAlias(const std::shared_ptr<Adjustment>& adjustment, XrTime time);
    void CleanupAliases(XrTime time);
    SpaceInfo(std::shared_ptr<SessionInfo>& session_, XrSpace space_, const XrReferenceSpaceCreateInfo& createInfo_);
    ~SpaceInfo();
};

template <typename Enum>
struct EnumHasher {
    using UnderLying = typename std::underlying_type<Enum>::type;
    std::size_t operator()(Enum val) const { return std::hash<UnderLying>()(static_cast<UnderLying>(val)); }
};

struct SessionInfo {
    SessionInfo() : instance(), session_handle(XR_NULL_HANDLE) {}
    SessionInfo(const std::shared_ptr<InstanceInfo>& instance_, XrSession session_)
        : instance(instance_), session_handle(session_) {}
    std::shared_ptr<InstanceInfo> Instance() const { return instance.lock(); }

    XrSession session_handle;
    std::weak_ptr<InstanceInfo> instance;
    std::unordered_map<SpaceInfo*, std::weak_ptr<SpaceInfo>> spaces;
    static constexpr XrTime kMinTimeInvalid = std::numeric_limits<XrTime>::min();
    using RefSpaceAdjustmentMap = std::unordered_map<XrReferenceSpaceType, AdjustmentTimeline, EnumHasher<XrReferenceSpaceType>>;
    RefSpaceAdjustmentMap adjustments;
    std::shared_ptr<Adjustment> GetAdjustment(XrReferenceSpaceType well_known, XrTime time);
};

using InstanceMap = std::unordered_map<XrInstance, std::shared_ptr<InstanceInfo>>;
using SessionMap = std::unordered_map<XrSession, std::shared_ptr<SessionInfo>>;
using SpaceMap = std::unordered_map<XrSpace, std::shared_ptr<SpaceInfo>>;

};  // namespace adjusted_reference_space_sublayer

#endif
