// Copyright (c) 2017-2020 The Khronos Group Inc.
// Copyright (c) 2017-2021 LunarG, Inc.
// Copyright (c) 2017-2021 PlutoVR Inc.
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
//
// Adjusted reference space impelementation as a sub-layer to the overlay layer

#include <algorithm>
#include <cassert>
#include <mutex>
#include "adjusted_ref_space.h"

namespace adjusted_reference_space_sublayer {

static InstanceMap g_instance_map;
static SessionMap g_session_map;
static SpaceMap g_space_map;
static std::mutex g_map_lock;
using Lock = std::lock_guard<std::mutex>;

static std::shared_ptr<Adjustment> FindInTimeline(const AdjustmentTimeline& timeline, XrTime time) {
    std::shared_ptr<Adjustment> found;
    if (!timeline.empty()) {
        auto found_it = timeline.lower_bound(time);
        const auto end_it = timeline.cend();
        if (found_it == end_it) {
            // timeline is not empty, so next to last is the one we want
            --found_it;
        } else if (found_it->first != time) {
            // The lower bound is past the sought time, so the current value is the previous value (or none)
            if (found_it != timeline.cbegin()) {
                // There is a previous value, so use it
                --found_it;
            } else {
                // there is no previous value so ensure we don't return one
                found_it = end_it;
            }
        }
        // *else* clause would be use the found_it value as is, and thus is empty

        if (found_it != end_it) {
            found = found_it->second;
        }
    }
    return found;
}

std::shared_ptr<InstanceInfo> InstanceFromSession(const std::shared_ptr<SessionInfo>& session) {
    if (session) {
        return session->Instance();
    }
    return std::shared_ptr<InstanceInfo>();
}

std::shared_ptr<SessionInfo> SessionFromSpace(const std::shared_ptr<SpaceInfo>& space) {
    if (space) {
        return space->Session();
    }
    return std::shared_ptr<SessionInfo>();
}

struct FoundSession {
    std::shared_ptr<InstanceInfo> instance = nullptr;
    std::shared_ptr<SessionInfo> session = nullptr;
    bool IsValid() const { return instance && session; }
    explicit operator bool() const { return IsValid(); }
    FoundSession(XrSession session_handle);
    FoundSession() = default;
};

// Only safe to construct when g_map_lock is held
FoundSession::FoundSession(XrSession session_handle) {
    auto session_info_it = g_session_map.find(session_handle);
    if (session_info_it != g_session_map.end()) {
        session = session_info_it->second;
        instance = InstanceFromSession(session);
    }
}

struct FoundSpace : public FoundSession {
    std::shared_ptr<SpaceInfo> space = nullptr;
    bool IsValid() const { return FoundSession::IsValid() && space; }
    explicit operator bool() const { return IsValid(); }
    FoundSpace::FoundSpace(XrSpace space_handle);
    FoundSpace::FoundSpace() = default;
};

FoundSpace::FoundSpace(XrSpace space_handle) : FoundSession() {
    auto space_info_it = g_space_map.find(space_handle);
    if (space_info_it != g_space_map.end()) {
        space = space_info_it->second;
        session = SessionFromSpace(space);
        instance = InstanceFromSession(session);
    }
}

void CreateInstance(XrInstance instance, XrGeneratedDispatchTable* instance_downchain) {
    auto instance_info = std::make_shared<InstanceInfo>(instance);
    auto insert_pair = g_instance_map.emplace(std::make_pair(instance, instance_info));
    assert(insert_pair.second);
    instance_info->downchain.Patch(instance_downchain);
}

static XRAPI_ATTR XrResult XRAPI_CALL DestroyInstancePatch(XrInstance instance) {
    PFN_xrDestroyInstance downchain_destroy = nullptr;
    {
        // Clean up the sublayer
        Lock map_guard(g_map_lock);
        auto instance_info_it = g_instance_map.find(instance);
        assert(instance_info_it != g_instance_map.end());

        std::shared_ptr<InstanceInfo>& instance_info = instance_info_it->second;
        assert(instance_info);
        downchain_destroy = instance_info->downchain.DestroyInstance;

        // Delete all the session info for this instance
        for (const auto& session_wp : instance_info->sessions) {
            auto session_info = session_wp.second.lock();
            assert(session_info);
            g_session_map.erase(session_info->session_handle);
        }
        g_instance_map.erase(instance_info_it);
    }

    // and call downchain
    XrResult result = XR_ERROR_HANDLE_INVALID;
    if (downchain_destroy) {
        result = downchain_destroy(instance);
    }
    return result;
}

static XRAPI_ATTR XrResult XRAPI_CALL CreateSessionPatch(XrInstance instance, const XrSessionCreateInfo* createInfo,
                                                         XrSession* session) {
    XrResult result = XR_ERROR_HANDLE_INVALID;

    PFN_xrCreateSession downchain_create = nullptr;
    {
        Lock map_guard(g_map_lock);
        auto instance_info_it = g_instance_map.find(instance);
        if (instance_info_it != g_instance_map.end()) {
            downchain_create = instance_info_it->second->downchain.CreateSession;
        }
    }
    if (downchain_create) {
        result = downchain_create(instance, createInfo, session);
        if (XR_SUCCEEDED(result)) {
            Lock map_guard(g_map_lock);
            auto instance_info_it = g_instance_map.find(instance);
            if (instance_info_it != g_instance_map.end()) {
                std::shared_ptr<InstanceInfo>& instance_info = instance_info_it->second;
                auto session_info = std::make_shared<SessionInfo>(instance_info, *session);
                auto insert_pair = g_session_map.emplace(std::make_pair(*session, session_info));
                assert(insert_pair.second);
                instance_info->sessions.emplace(session_info.get(), session_info);
            }
        }
    }
    return result;
}

static XRAPI_ATTR XrResult XRAPI_CALL DestroySessionPatch(XrSession session) {
    XrResult result = XR_ERROR_HANDLE_INVALID;
    PFN_xrDestroySession downchain_destroy = nullptr;
    {
        Lock map_guard(g_map_lock);
        FoundSession info(session);
        if (info) {
            downchain_destroy = info.instance->downchain.DestroySession;
            info.instance->sessions.erase(info.session.get());
            g_session_map.erase(session);
        }
    }

    if (downchain_destroy) {
        result = downchain_destroy(session);
    }

    return result;
}
static XRAPI_ATTR XrResult XRAPI_CALL CreateReferenceSpacePatch(XrSession session, const XrReferenceSpaceCreateInfo* createInfo,
                                                                XrSpace* space) {
    XrResult result = XR_ERROR_HANDLE_INVALID;
    PFN_xrCreateReferenceSpace downchain_create_rs = nullptr;
    {
        Lock map_guard(g_map_lock);
        FoundSession info(session);
        if (info) {
            downchain_create_rs = info.instance->downchain.CreateReferenceSpace;
        }
    }

    if (downchain_create_rs) {
        result = downchain_create_rs(session, createInfo, space);
    }

    if (XR_SUCCEEDED(result)) {
        Lock map_guard(g_map_lock);
        FoundSession info(session);
        if (info) {
            auto space_info = std::make_shared<SpaceInfo>(info.session, *space, *createInfo);
            info.session->spaces.emplace(std::make_pair(space_info.get(), space_info));
        }
    }

    return result;
}

static XRAPI_ATTR XrResult XRAPI_CALL LocateViewsPatch(XrSession session, const XrViewLocateInfo* viewLocateInfo,
                                                       XrViewState* viewState, uint32_t viewCapacityInput,
                                                       uint32_t* viewCountOutput, XrView* views) {
    XrResult result = XR_ERROR_HANDLE_INVALID;
    PFN_xrLocateViews downchain_locate = nullptr;
    {
        Lock map_guard(g_map_lock);
        FoundSession info(session);
        if (info) {
            downchain_locate = info.instance->downchain.LocateViews;
        }
        // Do space -> alias mapping
    }

    if (downchain_locate) {
        // WIP: needs to use touched up aliases
        result = downchain_locate(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);
    }

    // WIP: do we need post call cleanup?
    if (XR_SUCCEEDED(result)) {
        Lock map_guard(g_map_lock);
        FoundSession info(session);
        if (info) {
        }
    }

    return result;
}
static XRAPI_ATTR XrResult XRAPI_CALL LocateSpacePatch(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location) {
    XrResult result = XR_ERROR_HANDLE_INVALID;
    PFN_xrLocateSpace downchain_locate = nullptr;

    // Ensure that the spaces don't destruct during downchain operations (to protect aliases)
    std::shared_ptr<SpaceInfo> locked_space;
    std::shared_ptr<SpaceInfo> locked_base_space;
    std::shared_ptr<SpaceAlias> space_alias;
    std::shared_ptr<SpaceAlias> base_space_alias;
    {
        Lock map_guard(g_map_lock);
        FoundSpace info(space);
        if (info) {
            FoundSpace base_info(baseSpace);
            if (base_info) {
                assert(info.session == base_info.session);
                locked_space = info.space;
                locked_base_space = base_info.space;
                downchain_locate = info.instance->downchain.LocateSpace;

                space_alias = std::move(info.space->FindOrCreateAlias(*info.session, time));
                base_space_alias = std::move(base_info.space->FindOrCreateAlias(*base_info.session, time));
            }
        }
    }

    if (downchain_locate) {
        result = downchain_locate(space_alias, base_space_alias, time, location);
    }

    return result;
}
static XRAPI_ATTR XrResult XRAPI_CALL DestroySpacePatch(XrSpace space) {
    XrResult result = XR_ERROR_HANDLE_INVALID;
    PFN_xrDestroySpace downchain_destroy = nullptr;
    {
        Lock map_guard(g_map_lock);
        FoundSpace info(space);
        if (info) {
            downchain_destroy = info.instance->downchain.DestroySpace;
        }
        // WIP clean up the space refs
        info.session->spaces.erase(info.space.get());
    }

    if (downchain_destroy) {
        result = downchain_destroy(space);
    }

    if (XR_SUCCEEDED(result)) {
        Lock map_guard(g_map_lock);
        FoundSpace info(space);
        if (info) {
        }
    }

    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL LocateReferenceSpace(XrSession session, XrReferenceSpaceType referenceSpaceType, XrTime time,
                                                    XrSpaceLocation* location) {
    // WIP Implement Locate
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL AdjustReferenceSpace(XrSession session, XrReferenceSpaceType referenceSpaceType, XrTime time,
                                                    const XrPosef* adjustment) {
    // WIP Implement Adjust
    return XR_SUCCESS;
}

DispatchTable::DispatchTable()
    : DestroyInstance(DestroyInstancePatch),
      CreateSession(CreateSessionPatch),
      DestroySession(DestroySessionPatch),
      CreateReferenceSpace(CreateReferenceSpacePatch),
      LocateViews(LocateViewsPatch),
      LocateSpace(LocateSpacePatch),
      DestroySpace(DestroySpacePatch) {}

void DispatchTable::Patch(XrGeneratedDispatchTable* downchain) {
    // Only implement this locally, if not propagated from downchain.
    if (downchain->LocateReferenceSpaceEXTX || downchain->AdjustReferenceSpaceEXTX) return;

    downchain->LocateReferenceSpaceEXTX = LocateReferenceSpace;
    downchain->AdjustReferenceSpaceEXTX = AdjustReferenceSpace;

    std::swap(DestroyInstance, downchain->DestroyInstance);
    std::swap(CreateSession, downchain->CreateSession);
    std::swap(DestroySession, downchain->DestroySession);
    std::swap(CreateReferenceSpace, downchain->CreateReferenceSpace);
    std::swap(LocateViews, downchain->LocateViews);
    std::swap(LocateSpace, downchain->LocateSpace);
    std::swap(DestroySpace, downchain->DestroySpace);
}

SpaceInfo::AdjustmentAliases::iterator SpaceInfo::AddAlias(const std::shared_ptr<Adjustment>& adjustment, XrTime time) {
    return AdjustmentAliases::iterator();
}

void SpaceInfo::CleanupAliases(XrTime time) {}

SpaceInfo::SpaceInfo(std::shared_ptr<SessionInfo>& session_, XrSpace space_, const XrReferenceSpaceCreateInfo& createInfo)
    : space_handle(space_), session(session_), create_info(createInfo), aliases() {
    create_info.next = nullptr;  // Note: Shallow copy only
}

// Assumes a shared pointer for SessionInfo is held in the calling stack
// Returns a "busy" space alias;
std::shared_ptr<SpaceAlias> SpaceInfo::FindOrCreateAlias(SessionInfo& session, XrTime time) {
    std::shared_ptr<Adjustment> adjustment = session.GetAdjustment(create_info.referenceSpaceType, time);
    SpaceAlias* space_alias = nullptr;
    if (adjustment) {
        // only need an alias if there is an adjustment
        auto alias_it = aliases.find(adjustment.get());
        if (alias_it == aliases.end()) {
            alias_it = AddAlias(adjustment, time);
            CleanupAliases(time);
        }
        space_alias = alias_it->second.get();
    }
    return std::shared_ptr<SpaceAlias>(space_alias);
}

SpaceInfo::~SpaceInfo() {
    std::shared_ptr<SessionInfo> locked_session = Session();
    if (locked_session) {
        std::shared_ptr<InstanceInfo> locked_instance = locked_session->Instance();
        if (locked_instance) {
            for (const auto& alias : aliases) {
                const XrSpace alias_handle = alias.second->Handle();
                if (alias_handle != XR_NULL_HANDLE) {
                    locked_instance->downchain.DestroySpace(alias_handle);
                }
            }
        }
    }
}

std::shared_ptr<Adjustment> SessionInfo::GetAdjustment(XrReferenceSpaceType well_known, XrTime time) {
    AdjustmentTimeline& timeline = adjustments[well_known];
    if (timeline.empty()) {
        // Enter a default adjustment at T=0 (which is before any valid time)
        timeline.emplace(std::make_pair(XrTime(0), std::make_shared<Adjustment>()));
    }
    return FindInTimeline(timeline, time);
}

inline bool SpaceAlias::Valid(XrTime limit) const {
    bool result = false;
    if (alias_handle_ != XR_NULL_HANDLE) {
        auto locked = adjustment_.lock();
        if (locked && locked->time >= limit) {
            result = true;
        }
    }
    return result;
}

inline SpaceAlias& SpaceAlias::Set(XrSpace alias, std::shared_ptr<Adjustment>& adjustment) {
    *this = SpaceAlias(alias, adjustment);
    return *this;
}

};  // namespace adjusted_reference_space_sublayer