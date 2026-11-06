// A tiny OpenXR app to prove the mock runtime (../xrmock.c) works without any
// of Azahar for Frame's code: XR_KHR_vulkan_enable2 instance and device, a session,
// one 400x240 quad swapchain, 120 frames of acquire / wait / clear / fence /
// release / endFrame, and the frame_controller actions of ../README.md.
//
// Needs the OpenXR loader (XR_LOADER, default the SteamVR one) and
// libvulkan.so.1; both are opened with dlopen.  Run it through mockrun.sh
// (--bin build-xrmock/xrmock_test) or with XR_RUNTIME_JSON set to the mock's
// runtime.json.  Arguments are ignored (mockrun.sh passes --vr <rom>).
//
// With XRMOCK_TEST_INPUT=1 (and MOCK_XR_INPUT_SCRIPT=test/input.script) it also
// checks that the scripted input arrives.  Exit 0: every check passed.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#define XR_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#define FRAMES 120
#define W 400
#define H 240

static int gFail = 0;
#define CHECK(x)                                                         \
    do {                                                                 \
        if (!(x)) {                                                      \
            fprintf(stderr, "test: FAILED %s (line %d)\n", #x, __LINE__); \
            gFail++;                                                     \
        }                                                                \
    } while (0)
#define XR(x)                                                                  \
    do {                                                                       \
        XrResult r_ = (x);                                                     \
        if (XR_FAILED(r_)) {                                                   \
            fprintf(stderr, "test: %s = %d (line %d)\n", #x, (int)r_, __LINE__); \
            exit(2);                                                           \
        }                                                                      \
    } while (0)

static PFN_xrGetInstanceProcAddr xrGIPA;
static XrInstance gInst;
#define XRFN(name) PFN_##name name = (PFN_##name)load(#name)
static PFN_xrVoidFunction load(const char* name) {
    PFN_xrVoidFunction f = NULL;
    if (XR_FAILED(xrGIPA(gInst, name, &f)) || !f) {
        fprintf(stderr, "test: no %s\n", name);
        exit(2);
    }
    return f;
}

static XrPath path(PFN_xrStringToPath s, const char* p) {
    XrPath x;
    XR(s(gInst, p, &x));
    return x;
}

int main(void) {
    const char* loaderPath = getenv("XR_LOADER") ? getenv("XR_LOADER") : "/opt/steamvr/bin/linuxarm64/libopenxr_loader.so";
    void* lib = dlopen(loaderPath, RTLD_NOW);
    if (!lib) lib = dlopen("libopenxr_loader.so", RTLD_NOW);
    if (!lib) {
        fprintf(stderr, "test: no OpenXR loader (%s)\n", dlerror());
        return 2;
    }
    xrGIPA = (PFN_xrGetInstanceProcAddr)dlsym(lib, "xrGetInstanceProcAddr");
    PFN_xrCreateInstance xrCreateInstance = NULL;
    xrGIPA(XR_NULL_HANDLE, "xrCreateInstance", (PFN_xrVoidFunction*)&xrCreateInstance);

    const char* exts[] = {"XR_KHR_vulkan_enable2", "XR_VALVE_frame_controller_interaction"};
    XrInstanceCreateInfo ici = {XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy(ici.applicationInfo.applicationName, "xrmock_test");
    ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    ici.enabledExtensionCount = 2;
    ici.enabledExtensionNames = exts;
    XR(xrCreateInstance(&ici, &gInst));

    XRFN(xrGetSystem);
    XRFN(xrGetVulkanGraphicsRequirements2KHR);
    XRFN(xrCreateVulkanInstanceKHR);
    XRFN(xrGetVulkanGraphicsDevice2KHR);
    XRFN(xrCreateVulkanDeviceKHR);
    XRFN(xrCreateSession);
    XRFN(xrCreateReferenceSpace);
    XRFN(xrCreateActionSpace);
    XRFN(xrLocateSpace);
    XRFN(xrEnumerateSwapchainFormats);
    XRFN(xrCreateSwapchain);
    XRFN(xrEnumerateSwapchainImages);
    XRFN(xrAcquireSwapchainImage);
    XRFN(xrWaitSwapchainImage);
    XRFN(xrReleaseSwapchainImage);
    XRFN(xrPollEvent);
    XRFN(xrBeginSession);
    XRFN(xrEndSession);
    XRFN(xrRequestExitSession);
    XRFN(xrWaitFrame);
    XRFN(xrBeginFrame);
    XRFN(xrEndFrame);
    XRFN(xrStringToPath);
    XRFN(xrCreateActionSet);
    XRFN(xrCreateAction);
    XRFN(xrSuggestInteractionProfileBindings);
    XRFN(xrAttachSessionActionSets);
    XRFN(xrSyncActions);
    XRFN(xrGetActionStateBoolean);
    XRFN(xrGetActionStateFloat);
    XRFN(xrGetActionStateVector2f);
    XRFN(xrGetCurrentInteractionProfile);

    XrSystemGetInfo sgi = {XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId sys;
    XR(xrGetSystem(gInst, &sgi, &sys));

    // ---- Vulkan, through the runtime ----
    void* vkl = dlopen("libvulkan.so.1", RTLD_NOW);
    if (!vkl) {
        fprintf(stderr, "test: no libvulkan.so.1\n");
        return 2;
    }
    PFN_vkGetInstanceProcAddr vkGIPA = (PFN_vkGetInstanceProcAddr)dlsym(vkl, "vkGetInstanceProcAddr");
    XrGraphicsRequirementsVulkanKHR req = {XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    XR(xrGetVulkanGraphicsRequirements2KHR(gInst, sys, &req));

    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo vici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    vici.pApplicationInfo = &app;
    XrVulkanInstanceCreateInfoKHR xvi = {XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    xvi.systemId = sys;
    xvi.pfnGetInstanceProcAddr = vkGIPA;
    xvi.vulkanCreateInfo = &vici;
    VkInstance vki;
    VkResult vr;
    XR(xrCreateVulkanInstanceKHR(gInst, &xvi, &vki, &vr));
    CHECK(vr == VK_SUCCESS);

    XrVulkanGraphicsDeviceGetInfoKHR gdi = {XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    gdi.systemId = sys;
    gdi.vulkanInstance = vki;
    VkPhysicalDevice phys;
    XR(xrGetVulkanGraphicsDevice2KHR(gInst, &gdi, &phys));

#define VKI(name) PFN_##name name = (PFN_##name)vkGIPA(vki, #name)
    VKI(vkGetPhysicalDeviceQueueFamilyProperties);
    VKI(vkGetDeviceProcAddr);
    uint32_t nq = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, NULL);
    VkQueueFamilyProperties qp[16];
    if (nq > 16) nq = 16;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, qp);
    uint32_t fam = 0;
    for (uint32_t i = 0; i < nq; i++)
        if (qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            fam = i;
            break;
        }
    float prio = 1.0f;
    VkDeviceQueueCreateInfo dqi = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    dqi.queueFamilyIndex = fam;
    dqi.queueCount = 1;
    dqi.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &dqi;
    XrVulkanDeviceCreateInfoKHR xvd = {XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    xvd.systemId = sys;
    xvd.pfnGetInstanceProcAddr = vkGIPA;
    xvd.vulkanPhysicalDevice = phys;
    xvd.vulkanCreateInfo = &dci;
    VkDevice dev;
    XR(xrCreateVulkanDeviceKHR(gInst, &xvd, &dev, &vr));
    CHECK(vr == VK_SUCCESS);

#define VKD(name) PFN_##name name = (PFN_##name)vkGetDeviceProcAddr(dev, #name)
    VKD(vkGetDeviceQueue);
    VKD(vkCreateCommandPool);
    VKD(vkAllocateCommandBuffers);
    VKD(vkBeginCommandBuffer);
    VKD(vkEndCommandBuffer);
    VKD(vkCmdPipelineBarrier);
    VKD(vkCmdClearColorImage);
    VKD(vkQueueSubmit);
    VKD(vkCreateFence);
    VKD(vkWaitForFences);
    VKD(vkResetFences);
    VKD(vkResetCommandBuffer);
    VkQueue queue;
    vkGetDeviceQueue(dev, fam, 0, &queue);

    // ---- session ----
    XrGraphicsBindingVulkan2KHR gb = {XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    gb.instance = vki;
    gb.physicalDevice = phys;
    gb.device = dev;
    gb.queueFamilyIndex = fam;
    gb.queueIndex = 0;
    XrSessionCreateInfo sci = {XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &gb;
    sci.systemId = sys;
    XrSession session;
    XR(xrCreateSession(gInst, &sci, &session));

    XrReferenceSpaceCreateInfo rsi = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rsi.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rsi.poseInReferenceSpace.orientation.w = 1;
    XrSpace local;
    XR(xrCreateReferenceSpace(session, &rsi, &local));

    // ---- actions on the frame controller profile ----
    XrActionSetCreateInfo asi = {XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy(asi.actionSetName, "test");
    strcpy(asi.localizedActionSetName, "Test");
    XrActionSet set;
    XR(xrCreateActionSet(gInst, &asi, &set));
    XrPath hands[2] = {path(xrStringToPath, "/user/hand/left"), path(xrStringToPath, "/user/hand/right")};
    XrAction aBtn, aStick, aTrig, aAim;
    struct {
        const char* name;
        XrActionType type;
        XrAction* out;
    } defs[] = {{"button_a", XR_ACTION_TYPE_BOOLEAN_INPUT, &aBtn},
                {"stick", XR_ACTION_TYPE_VECTOR2F_INPUT, &aStick},
                {"trigger", XR_ACTION_TYPE_FLOAT_INPUT, &aTrig},
                {"aim", XR_ACTION_TYPE_POSE_INPUT, &aAim}};
    for (int i = 0; i < 4; i++) {
        XrActionCreateInfo ai = {XR_TYPE_ACTION_CREATE_INFO};
        ai.actionType = defs[i].type;
        strcpy(ai.actionName, defs[i].name);
        strcpy(ai.localizedActionName, defs[i].name);
        ai.countSubactionPaths = 2;
        ai.subactionPaths = hands;
        XR(xrCreateAction(set, &ai, defs[i].out));
    }
    XrActionSuggestedBinding sb[] = {{aBtn, path(xrStringToPath, "/user/hand/right/input/a/click")},
                                     {aStick, path(xrStringToPath, "/user/hand/left/input/thumbstick")},
                                     {aStick, path(xrStringToPath, "/user/hand/right/input/thumbstick")},
                                     {aTrig, path(xrStringToPath, "/user/hand/left/input/trigger/value")},
                                     {aTrig, path(xrStringToPath, "/user/hand/right/input/trigger/value")},
                                     {aAim, path(xrStringToPath, "/user/hand/left/input/aim/pose")},
                                     {aAim, path(xrStringToPath, "/user/hand/right/input/aim/pose")}};
    XrInteractionProfileSuggestedBinding sug = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    sug.interactionProfile = path(xrStringToPath, "/interaction_profiles/valve/frame_controller");
    sug.suggestedBindings = sb;
    sug.countSuggestedBindings = 7;
    XR(xrSuggestInteractionProfileBindings(gInst, &sug));
    XrSessionActionSetsAttachInfo att = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    att.countActionSets = 1;
    att.actionSets = &set;
    XR(xrAttachSessionActionSets(session, &att));
    XrActionSpaceCreateInfo aci = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
    aci.action = aAim;
    aci.subactionPath = hands[1];
    aci.poseInActionSpace.orientation.w = 1;
    XrSpace aimSpace;
    XR(xrCreateActionSpace(session, &aci, &aimSpace));

    // ---- swapchain ----
    uint32_t nf = 0;
    xrEnumerateSwapchainFormats(session, 0, &nf, NULL);
    int64_t fmts[32];
    if (nf > 32) nf = 32;
    xrEnumerateSwapchainFormats(session, nf, &nf, fmts);
    int have = 0;
    for (uint32_t i = 0; i < nf; i++) have |= fmts[i] == VK_FORMAT_R8G8B8A8_SRGB;
    CHECK(have);
    XrSwapchainCreateInfo wci = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
    wci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    wci.format = VK_FORMAT_R8G8B8A8_SRGB;
    wci.sampleCount = 1;
    wci.width = W;
    wci.height = H;
    wci.faceCount = 1;
    wci.arraySize = 1;
    wci.mipCount = 1;
    XrSwapchain sc;
    XR(xrCreateSwapchain(session, &wci, &sc));
    uint32_t nimg = 0;
    xrEnumerateSwapchainImages(sc, 0, &nimg, NULL);
    XrSwapchainImageVulkan2KHR imgs[8];
    if (nimg > 8) nimg = 8;
    for (uint32_t i = 0; i < nimg; i++) imgs[i] = (XrSwapchainImageVulkan2KHR){XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR};
    XR(xrEnumerateSwapchainImages(sc, nimg, &nimg, (XrSwapchainImageBaseHeader*)imgs));
    CHECK(nimg >= 2);

    VkCommandPoolCreateInfo cpi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpi.queueFamilyIndex = fam;
    VkCommandPool pool;
    vkCreateCommandPool(dev, &cpi, NULL, &pool);
    VkCommandBufferAllocateInfo cbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbi.commandPool = pool;
    cbi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbi.commandBufferCount = 1;
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(dev, &cbi, &cb);
    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence;
    vkCreateFence(dev, &fci, NULL, &fence);

    // ---- frame loop ----
    int running = 0, done = 0, frames = 0, sawA = 0, sawStick = 0, sawTrig = 0, sawAim = 0, sawProfile = 0, exitAsked = 0;
    while (!done) {
        XrEventDataBuffer ev = {XR_TYPE_EVENT_DATA_BUFFER};
        while (xrPollEvent(gInst, &ev) == XR_SUCCESS) {
            if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                XrSessionState st = ((XrEventDataSessionStateChanged*)&ev)->state;
                if (st == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo bi = {XR_TYPE_SESSION_BEGIN_INFO};
                    bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    XR(xrBeginSession(session, &bi));
                    running = 1;
                } else if (st == XR_SESSION_STATE_STOPPING) {
                    XR(xrEndSession(session));
                    running = 0;
                    done = 1;
                }
            }
            ev = (XrEventDataBuffer){XR_TYPE_EVENT_DATA_BUFFER};
        }
        if (!running) continue;

        XrFrameState fs = {XR_TYPE_FRAME_STATE};
        XR(xrWaitFrame(session, NULL, &fs));
        XR(xrBeginFrame(session, NULL));

        XrActiveActionSet act = {set, XR_NULL_PATH};
        XrActionsSyncInfo syn = {XR_TYPE_ACTIONS_SYNC_INFO};
        syn.countActiveActionSets = 1;
        syn.activeActionSets = &act;
        XR(xrSyncActions(session, &syn));
        XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
        gi.subactionPath = hands[1];
        gi.action = aBtn;
        XrActionStateBoolean sbool = {XR_TYPE_ACTION_STATE_BOOLEAN};
        XR(xrGetActionStateBoolean(session, &gi, &sbool));
        sawA |= sbool.isActive && sbool.currentState;
        gi.action = aStick;
        XrActionStateVector2f svec = {XR_TYPE_ACTION_STATE_VECTOR2F};
        gi.subactionPath = hands[0];
        XR(xrGetActionStateVector2f(session, &gi, &svec));
        sawStick |= svec.isActive && fabsf(svec.currentState.x - 0.5f) < 1e-3f && fabsf(svec.currentState.y + 0.25f) < 1e-3f;
        gi.action = aTrig;
        gi.subactionPath = hands[1];
        XrActionStateFloat sfl = {XR_TYPE_ACTION_STATE_FLOAT};
        XR(xrGetActionStateFloat(session, &gi, &sfl));
        sawTrig |= sfl.isActive && fabsf(sfl.currentState - 0.8f) < 1e-3f;
        XrSpaceLocation loc = {XR_TYPE_SPACE_LOCATION};
        XR(xrLocateSpace(aimSpace, local, fs.predictedDisplayTime, &loc));
        sawAim |= fabsf(loc.pose.position.x - 0.1f) < 1e-3f && fabsf(loc.pose.position.y - 0.2f) < 1e-3f;
        if (!sawProfile) {
            XrInteractionProfileState ps = {XR_TYPE_INTERACTION_PROFILE_STATE};
            XR(xrGetCurrentInteractionProfile(session, hands[1], &ps));
            sawProfile = ps.interactionProfile != XR_NULL_PATH;
        }

        // Draw: clear the quad's image to a colour that changes every frame.
        uint32_t idx;
        XR(xrAcquireSwapchainImage(sc, NULL, &idx));
        XrSwapchainImageWaitInfo wi = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wi.timeout = XR_INFINITE_DURATION;
        XR(xrWaitSwapchainImage(sc, &wi));
        VkImage image = imgs[idx].image;
        vkResetCommandBuffer(cb, 0);
        VkCommandBufferBeginInfo bgi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bgi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &bgi);
        VkImageMemoryBarrier bar = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        bar.srcQueueFamilyIndex = bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bar.image = image;
        bar.subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        bar.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        bar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bar.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &bar);
        VkClearColorValue clr = {{(frames % 60) / 60.0f, 0.25f, 1.0f - (frames % 60) / 60.0f, 1.0f}};
        vkCmdClearColorImage(cb, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clr, 1, &bar.subresourceRange);
        bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bar.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        bar.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &bar);
        vkEndCommandBuffer(cb);
        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        vkResetFences(dev, 1, &fence);
        vkQueueSubmit(queue, 1, &si, fence);
        vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);  // release only finished work
        XR(xrReleaseSwapchainImage(sc, NULL));

        XrCompositionLayerQuad q = {XR_TYPE_COMPOSITION_LAYER_QUAD};
        q.space = local;
        q.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        q.subImage.swapchain = sc;
        q.subImage.imageRect = (XrRect2Di){{0, 0}, {W, H}};
        q.pose.orientation.w = 1;
        q.pose.position.z = -2.0f;
        q.size = (XrExtent2Df){1.6f, 0.96f};
        const XrCompositionLayerBaseHeader* layers[1] = {(XrCompositionLayerBaseHeader*)&q};
        XrFrameEndInfo fe = {XR_TYPE_FRAME_END_INFO};
        fe.displayTime = fs.predictedDisplayTime;
        fe.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        fe.layerCount = 1;
        fe.layers = layers;
        XR(xrEndFrame(session, &fe));

        if (++frames == FRAMES && !exitAsked) {
            exitAsked = 1;
            XR(xrRequestExitSession(session));
        }
    }

    CHECK(frames >= FRAMES);
    CHECK(sawProfile);
    if (getenv("XRMOCK_TEST_INPUT")) {
        CHECK(sawA);
        CHECK(sawStick);
        CHECK(sawTrig);
        CHECK(sawAim);
    }
    fprintf(stderr, "test: %d frames, a=%d stick=%d trigger=%d aim=%d profile=%d: %s\n", frames, sawA, sawStick, sawTrig, sawAim, sawProfile,
            gFail ? "FAILED" : "ok");
    return gFail ? 1 : 0;
}
