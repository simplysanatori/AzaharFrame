// A mock OpenXR runtime for testing an OpenXR app's frame loop without a
// headset.  Loaded by the OpenXR loader through XR_RUNTIME_JSON (see
// runtime.json.in), in the app's process.
//
// Ported from GalaxyFrame's tools/xrmock (Unlicense), which was written for
// GalaxyFrame's xr_app.cpp.  Azahar for Frame's changes: Vulkan only, thread-safe
// (the app calls it from several threads), real action state with scripted
// input, action-space and reference-space poses, quad layer details, swapchain
// image dumps to PNG, and a per-second summary in the trace.
//
// It keeps the call discipline of a real runtime where the app can get it
// wrong (swapchain acquire / wait / release order, images in flight, frame
// call order, extensions and profiles the runtime does not offer) and a frame
// clock, and writes a trace of the calls that decide how the app paces:
// swapchain creation, acquire, release, frame begin / end with the layers.
//
// Environment:
//   MOCK_XR_TRACE=<file>      trace output (none: no trace)
//   MOCK_XR_SECONDS=<s>       run time, then the session stops (20; 0: never)
//   MOCK_XR_HZ=<hz>           display refresh (120)
//   MOCK_XR_SWAY=0            keep the head still (default: it turns slowly
//                             from side to side, so poses located for
//                             different times differ)
//   MOCK_XR_SPACEWARP=1       offer XR_FB_space_warp
//   MOCK_XR_EXTENSIONS=a,b    replace the list of extensions offered
//   MOCK_XR_SKIP=1            a frame begun late loses refreshes (the display
//                             times jump), as on a headset
//   MOCK_XR_END_GPU=1         xrEndFrame waits for the app's queue to go idle, as
//                             SteamVR's did on the Frame with a GPU-heavy game
//                             (13-23 ms xrEndFrame calls, under the app's queue lock)
//   MOCK_XR_BUSY_MS=<ms>      a release that waits longer than this for the
//                             app's queue to go idle is an error (off unless
//                             set: it stalls the app's queue)
//   MOCK_XR_INPUT_SCRIPT=<f>  scripted controller input, see README.md
//   MOCK_XR_TRACE_POSE=1      quad layer positions in the trace
//   MOCK_XR_DUMP_DIR=<dir>    write swapchain images as PNG into this dir
//   MOCK_XR_DUMP_FRAME=<n|last>  the first image of each swapchain released at
//                             or after frame n, or (default) the last image
//                             released, written when the session ends; a
//                             comma list n1,n2,... dumps once after each
//   MOCK_XR_DUMP_SHOWN=t1,t2,...  at the first endFrame at or after each time
//                             (s since the session began), the image each
//                             submitted quad shows
//
// Checks (ERROR lines in the trace, and a summary on stderr when the session
// ends): acquire / wait / release order per swapchain; frame call order; a
// layer that submits a swapchain with no image released; with
// MOCK_XR_BUSY_MS, that the app's GPU work had finished at each release; that
// both eyes of a projection layer come from one xrLocateViews.
#define _GNU_SOURCE
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>

#include "xrmock_gfx.h"

// ---- trace, lock, clock ----------------------------------------------------

static pthread_mutex_t gMu = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#define LOCK() pthread_mutex_lock(&gMu)
#define UNLOCK() pthread_mutex_unlock(&gMu)

static FILE* gTrace;
static int gFrame = 0;
static int gErrors = 0;  // ERROR lines in the trace

static void vtrace(const char* fmt, va_list ap) {
    if (!gTrace) return;
    LOCK();
    fprintf(gTrace, "f%d ", gFrame);
    vfprintf(gTrace, fmt, ap);
    fputc('\n', gTrace);
    fflush(gTrace);
    UNLOCK();
}
static void trace(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vtrace(fmt, ap);
    va_end(ap);
}
void mock_error(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LOCK();
    gErrors++;
    UNLOCK();
    trace("ERROR %s", buf);
    if (!gTrace) fprintf(stderr, "xrmock: ERROR %s\n", buf);
}

static int64_t nowNs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000ll + ts.tv_nsec;
}

static const MockGfx* const kGfxTables[] = {&kMockGfxVk};
static const MockGfx* gGfx = NULL;  // the one the session is bound to

// ---- poses -----------------------------------------------------------------

static XrQuaternionf qmul(XrQuaternionf a, XrQuaternionf b) {
    return (XrQuaternionf){a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                           a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
static XrQuaternionf qconj(XrQuaternionf q) { return (XrQuaternionf){-q.x, -q.y, -q.z, q.w}; }
static XrVector3f qrot(XrQuaternionf q, XrVector3f v) {
    XrQuaternionf p = {v.x, v.y, v.z, 0};
    XrQuaternionf r = qmul(qmul(q, p), qconj(q));
    return (XrVector3f){r.x, r.y, r.z};
}
static XrPosef pmul(XrPosef a, XrPosef b) {
    XrVector3f t = qrot(a.orientation, b.position);
    return (XrPosef){qmul(a.orientation, b.orientation), {a.position.x + t.x, a.position.y + t.y, a.position.z + t.z}};
}
static XrPosef pinv(XrPosef a) {
    XrQuaternionf c = qconj(a.orientation);
    XrVector3f t = qrot(c, a.position);
    return (XrPosef){c, {-t.x, -t.y, -t.z}};
}
static XrPosef pident(void) { return (XrPosef){{0, 0, 0, 1}, {0, 0, 0}}; }
// yaw (left positive) then pitch (up positive), degrees; -Z forward.
static XrQuaternionf qyawpitch(float yawDeg, float pitchDeg) {
    float y = yawDeg * 3.14159265f / 360.0f, p = pitchDeg * 3.14159265f / 360.0f;
    XrQuaternionf qy = {0, sinf(y), 0, cosf(y)}, qp = {sinf(p), 0, 0, cosf(p)};
    return qmul(qy, qp);
}

static int gSway = 1;
static int64_t gT0 = 0, gPeriod = 8333333, gEndAt = 0;
// Sessions begun so far. A later session (an app that ended its session and made a new one)
// keeps the first one's timeline: script times, shown dumps and MOCK_XR_SECONDS count from the
// first xrBeginSession; the refresh count is resynced (gResync) at its first xrWaitFrame.
static int gSessionsBegun = 0, gResync = 0;

// The head: 1.6 m up, turning about the vertical axis.
static XrPosef headAt(XrTime t) {
    float yaw = 0;
    if (gSway) yaw = (float)(0.3 * sin((double)(t - gT0) / 1e9 * 2.0 * 3.14159265 * 0.5));
    return (XrPosef){{0, sinf(yaw * 0.5f), 0, cosf(yaw * 0.5f)}, {0, 1.6f, 0}};
}

// ---- paths -----------------------------------------------------------------

static XrPath gPaths[1024];
static char gPathStr[1024][128];
static int gPathCount = 0;

static const char* pathStr(XrPath p) {
    for (int k = 0; k < gPathCount; k++)
        if (gPaths[k] == p) return gPathStr[k];
    return "";
}

// ---- extensions ------------------------------------------------------------

static char gExtNames[64][XR_MAX_EXTENSION_NAME_SIZE];
static int gExtCount = 0;
static int gFrameControllerExt = 0;  // XR_VALVE_frame_controller_interaction enabled
static int gCylinderExt = 0;        // XR_KHR_composition_layer_cylinder enabled

static XrResult XRAPI_CALL mEnumExt(const char* layer, uint32_t cap, uint32_t* count, XrExtensionProperties* props) {
    (void)layer;
    *count = (uint32_t)gExtCount;
    if (cap == 0) return XR_SUCCESS;
    if (cap < (uint32_t)gExtCount) return XR_ERROR_SIZE_INSUFFICIENT;
    for (int i = 0; i < gExtCount; i++) {
        memset(props[i].extensionName, 0, sizeof(props[i].extensionName));
        strcpy(props[i].extensionName, gExtNames[i]);
        props[i].extensionVersion = 1;
    }
    return XR_SUCCESS;
}

static XrResult XRAPI_CALL mCreateInstance(const XrInstanceCreateInfo* ci, XrInstance* out) {
    char line[2048] = "";
    XrResult res = XR_SUCCESS;
    for (uint32_t i = 0; i < ci->enabledExtensionCount; i++) {
        const char* name = ci->enabledExtensionNames[i];
        int known = 0;
        for (int k = 0; k < gExtCount; k++) known |= !strcmp(gExtNames[k], name);
        if (!known) {
            mock_error("createInstance with extension %s, which the runtime does not offer", name);
            res = XR_ERROR_EXTENSION_NOT_PRESENT;
        }
        if (!strcmp(name, "XR_VALVE_frame_controller_interaction")) gFrameControllerExt = 1;
        if (!strcmp(name, "XR_KHR_composition_layer_cylinder")) gCylinderExt = 1;
        if (strlen(line) + strlen(name) + 2 < sizeof(line)) {
            strcat(line, " ");
            strcat(line, name);
        }
    }
    trace("createInstance extensions:%s", line);
    if (res != XR_SUCCESS) return res;
    *out = (XrInstance)(uintptr_t)1;
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mDestroyInstance(XrInstance i) {
    (void)i;
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mResultToString(XrInstance i, XrResult r, char buf[XR_MAX_RESULT_STRING_SIZE]) {
    (void)i;
    snprintf(buf, XR_MAX_RESULT_STRING_SIZE, "XrResult_%d", (int)r);
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mGetInstanceProps(XrInstance i, XrInstanceProperties* p) {
    (void)i;
    p->runtimeVersion = XR_MAKE_VERSION(0, 1, 0);
    strcpy(p->runtimeName, "xrmock");
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mGetSystem(XrInstance i, const XrSystemGetInfo* gi, XrSystemId* id) {
    (void)i;
    (void)gi;
    *id = 1;
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mGetSystemProps(XrInstance i, XrSystemId s, XrSystemProperties* p) {
    (void)i;
    (void)s;
    strcpy(p->systemName, "mock frame");
    p->graphicsProperties.maxSwapchainImageWidth = 4096;
    p->graphicsProperties.maxSwapchainImageHeight = 4096;
    p->graphicsProperties.maxLayerCount = 16;
    p->trackingProperties.orientationTracking = XR_TRUE;
    p->trackingProperties.positionTracking = XR_TRUE;
    for (XrBaseOutStructure* n = (XrBaseOutStructure*)p->next; n; n = n->next) {
        if (n->type == XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT) {
            ((XrSystemEyeGazeInteractionPropertiesEXT*)n)->supportsEyeGazeInteraction = XR_TRUE;
        }
        if (n->type == XR_TYPE_SYSTEM_SPACE_WARP_PROPERTIES_FB) {
            XrSystemSpaceWarpPropertiesFB* w = (XrSystemSpaceWarpPropertiesFB*)n;
            w->recommendedMotionVectorImageRectWidth = 420;
            w->recommendedMotionVectorImageRectHeight = 440;
        }
    }
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mEnumViewConfigs(XrInstance i, XrSystemId s, uint32_t cap, uint32_t* count, XrViewConfigurationType* t) {
    (void)i;
    (void)s;
    *count = 1;
    if (cap && t) t[0] = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mGetViewConfigProps(XrInstance i, XrSystemId s, XrViewConfigurationType t, XrViewConfigurationProperties* p) {
    (void)i;
    (void)s;
    p->viewConfigurationType = t;
    p->fovMutable = XR_FALSE;
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mEnumBlend(XrInstance i, XrSystemId s, XrViewConfigurationType t, uint32_t cap, uint32_t* count, XrEnvironmentBlendMode* m) {
    (void)i;
    (void)s;
    (void)t;
    // XRMOCK_ALPHA_BLEND=1 also offers alpha blending, as SteamVR on the Frame is said to.
    const char* alpha = getenv("XRMOCK_ALPHA_BLEND");
    const int with_alpha = alpha && alpha[0] == '1';
    *count = with_alpha ? 2 : 1;
    if (cap && m) {
        m[0] = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        if (with_alpha && cap > 1) m[1] = XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
    }
    return XR_SUCCESS;
}

// ---- spaces ----------------------------------------------------------------

enum { SP_LOCAL, SP_STAGE, SP_VIEW, SP_ACTION };
typedef struct {
    int kind;
    int act;      // SP_ACTION: the action index
    XrPath sub;   // SP_ACTION: /user/hand/left or right
    XrPosef offset;
} Space;
#define MAXSP 256
static Space gSp[MAXSP];
static int gSpCount = 0;
static const char* kSpaceNames[] = {"local", "stage", "view", "action"};

static XrSpace newSpace(int kind, int act, XrPath sub, XrPosef off) {
    LOCK();
    if (gSpCount >= MAXSP) {
        UNLOCK();
        return XR_NULL_HANDLE;
    }
    gSp[gSpCount] = (Space){kind, act, sub, off};
    XrSpace h = (XrSpace)(uintptr_t)(100 + gSpCount++);
    UNLOCK();
    return h;
}
static Space* spaceOf(XrSpace s) {
    uintptr_t i = (uintptr_t)s;
    if (i < 100 || i - 100 >= (uintptr_t)gSpCount) return NULL;
    return &gSp[i - 100];
}

// ---- actions and scripted input --------------------------------------------

typedef struct {
    char name[XR_MAX_ACTION_NAME_SIZE];
    XrActionType type;
} Act;
#define MAXACT 256
static Act gAct[MAXACT];
static int gActCount = 0;

typedef struct {
    int act;
    int profile;  // index into gProfiles
    char path[128];
} Bind;
#define MAXBIND 2048
static Bind gBind[MAXBIND];
static int gBindCount = 0;

static const char* kProfiles[] = {"/interaction_profiles/valve/frame_controller", "/interaction_profiles/khr/simple_controller",
                                  "/interaction_profiles/oculus/touch_controller", "/interaction_profiles/meta/touch_controller_plus",
                                  "/interaction_profiles/valve/index_controller"};
#define NPROFILES 5
static int gProfileSuggested[NPROFILES];
static int gProfile = -1;  // the active profile once the action sets are attached
static int gAttached = 0, gSynced = 0;

// Injected input, by full input path.
typedef struct {
    char path[128];
    double v[2];
    XrPosef pose;
    int isPose;
} In;
#define MAXIN 256
static In gIn[MAXIN], gInPrev[MAXIN];
static int gInCount = 0;

typedef struct {
    double when;  // seconds since the session began, or
    int frame;    // a frame number (when < 0)
    char path[128];
    int n;
    double v[7];
    int applied;
} Ev;
#define MAXEV 4096
static Ev gEv[MAXEV];
static int gEvCount = 0;

static In* inFind(In* tab, const char* path) {
    for (int i = 0; i < gInCount; i++)
        if (!strcmp(tab[i].path, path)) return &tab[i];
    return NULL;
}
static In* inGet(const char* path) {
    In* in = inFind(gIn, path);
    if (in) return in;
    if (gInCount >= MAXIN) return NULL;
    memset(&gIn[gInCount], 0, sizeof(In));
    memset(&gInPrev[gInCount], 0, sizeof(In));
    strncpy(gIn[gInCount].path, path, sizeof(gIn[0].path) - 1);
    strncpy(gInPrev[gInCount].path, path, sizeof(gInPrev[0].path) - 1);
    gIn[gInCount].pose = gInPrev[gInCount].pose = pident();
    return &gIn[gInCount++];
}

// A scalar value of an input path: exact, or a component (/x /y) of a 2D one.
static int inValue(In* tab, const char* path, double* x, double* y) {
    In* in = inFind(tab, path);
    if (in && !in->isPose) {
        *x = in->v[0];
        *y = in->v[1];
        return 1;
    }
    size_t n = strlen(path);
    if (n > 2 && (!strcmp(path + n - 2, "/x") || !strcmp(path + n - 2, "/y"))) {
        char parent[128];
        memcpy(parent, path, n - 2);
        parent[n - 2] = 0;
        in = inFind(tab, parent);
        if (in && !in->isPose) {
            *x = path[n - 1] == 'x' ? in->v[0] : in->v[1];
            *y = 0;
            return 1;
        }
    }
    return 0;
}

static void loadScript(const char* file) {
    FILE* f = fopen(file, "r");
    if (!f) {
        fprintf(stderr, "xrmock: cannot read MOCK_XR_INPUT_SCRIPT %s\n", file);
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), f) && gEvCount < MAXEV) {
        char* hash = strchr(line, '#');
        if (hash) *hash = 0;
        char* tok[16];
        int nt = 0;
        for (char* t = strtok(line, " \t\r\n"); t && nt < 16; t = strtok(NULL, " \t\r\n")) tok[nt++] = t;
        if (nt < 3) continue;
        Ev* e = &gEv[gEvCount];
        memset(e, 0, sizeof(*e));
        if (tok[0][0] == 'f') {
            e->frame = atoi(tok[0] + 1);
            e->when = -1;
        } else {
            e->when = atof(tok[0]);
            e->frame = -1;
        }
        strncpy(e->path, tok[1], sizeof(e->path) - 1);
        for (int i = 2; i < nt && e->n < 7; i++) {
            if (!strcmp(tok[i], "true")) e->v[e->n++] = 1;
            else if (!strcmp(tok[i], "false")) e->v[e->n++] = 0;
            else e->v[e->n++] = atof(tok[i]);
        }
        gEvCount++;
    }
    fclose(f);
}

// Applies the script events that are due; at a sync.
static void applyScript(void) {
    double now = gT0 ? (double)(nowNs() - gT0) / 1e9 : 0.0;
    for (int i = 0; i < gInCount; i++) gInPrev[i] = gIn[i];
    for (int i = 0; i < gEvCount; i++) {
        Ev* e = &gEv[i];
        if (e->applied) continue;
        if (e->when >= 0 ? now < e->when : gFrame < e->frame) continue;
        e->applied = 1;
        In* in = inGet(e->path);
        if (!in) continue;
        size_t n = strlen(e->path);
        if (n > 5 && !strcmp(e->path + n - 5, "/pose")) {
            // pos x y z, then nothing, "yaw pitch" in degrees, or a quaternion
            in->isPose = 1;
            in->pose = pident();
            if (e->n >= 3) in->pose.position = (XrVector3f){(float)e->v[0], (float)e->v[1], (float)e->v[2]};
            if (e->n == 5) in->pose.orientation = qyawpitch((float)e->v[3], (float)e->v[4]);
            if (e->n == 7) in->pose.orientation = (XrQuaternionf){(float)e->v[3], (float)e->v[4], (float)e->v[5], (float)e->v[6]};
            trace("input %s pose %.2f %.2f %.2f", e->path, in->pose.position.x, in->pose.position.y, in->pose.position.z);
        } else {
            in->v[0] = e->n > 0 ? e->v[0] : 0;
            in->v[1] = e->n > 1 ? e->v[1] : 0;
            trace("input %s %.3f %.3f", e->path, in->v[0], in->v[1]);
        }
        int bound = 0;
        for (int b = 0; b < gBindCount; b++) {
            if (gBind[b].profile != gProfile) continue;
            const char* bp = gBind[b].path;
            size_t bl = strlen(bp);
            if (!strcmp(bp, e->path) || (!strncmp(bp, e->path, bl) && e->path[bl] == '/') ||
                (!strncmp(bp, e->path, strlen(e->path)) && bp[strlen(e->path)] == '/'))
                bound = 1;
        }
        if (!bound) trace("WARN input %s is bound by no action", e->path);
    }
}

static XrResult XRAPI_CALL mCreateActionSet(XrInstance i, const XrActionSetCreateInfo* ci, XrActionSet* o) {
    (void)i;
    (void)ci;
    *o = (XrActionSet)(uintptr_t)3;
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mCreateAction(XrActionSet s, const XrActionCreateInfo* ci, XrAction* o) {
    (void)s;
    LOCK();
    if (gActCount >= MAXACT) {
        UNLOCK();
        return XR_ERROR_LIMIT_REACHED;
    }
    for (int i = 0; i < gActCount; i++) {
        if (!strcmp(gAct[i].name, ci->actionName)) {
            UNLOCK();
            mock_error("createAction %s twice", ci->actionName);
            return XR_ERROR_NAME_DUPLICATED;
        }
    }
    strncpy(gAct[gActCount].name, ci->actionName, XR_MAX_ACTION_NAME_SIZE - 1);
    gAct[gActCount].type = ci->actionType;
    *o = (XrAction)(uintptr_t)(200 + gActCount++);
    UNLOCK();
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mDestroyHandle(uint64_t h) {
    (void)h;
    return XR_SUCCESS;
}
// The app has one action set: destroying it frees its action names and bindings, so a new
// session can make, suggest and attach them again (as a runtime does).
static XrResult XRAPI_CALL mDestroyActionSet(XrActionSet h) {
    LOCK();
    gActCount = 0;
    gBindCount = 0;
    memset(gProfileSuggested, 0, sizeof(gProfileSuggested));
    gAttached = 0;
    gProfile = -1;
    trace("destroyActionSet");
    UNLOCK();
    return mDestroyHandle((uint64_t)(uintptr_t)h);
}
static XrResult XRAPI_CALL mDestroyAction(XrAction h) { return mDestroyHandle((uint64_t)(uintptr_t)h); }

static XrResult XRAPI_CALL mStringToPath(XrInstance i, const char* s, XrPath* p) {
    (void)i;
    LOCK();
    for (int k = 0; k < gPathCount; k++)
        if (!strcmp(gPathStr[k], s)) {
            *p = gPaths[k];
            UNLOCK();
            return XR_SUCCESS;
        }
    if (gPathCount >= 1024) {
        UNLOCK();
        return XR_ERROR_LIMIT_REACHED;
    }
    strncpy(gPathStr[gPathCount], s, 127);
    gPaths[gPathCount] = (XrPath)(5000 + gPathCount);
    *p = gPaths[gPathCount++];
    UNLOCK();
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mPathToString(XrInstance i, XrPath p, uint32_t cap, uint32_t* len, char* buf) {
    (void)i;
    LOCK();
    for (int k = 0; k < gPathCount; k++)
        if (gPaths[k] == p) {
            *len = (uint32_t)strlen(gPathStr[k]) + 1;
            if (cap) strncpy(buf, gPathStr[k], cap);
            UNLOCK();
            return XR_SUCCESS;
        }
    UNLOCK();
    return XR_ERROR_PATH_INVALID;
}

static XrResult XRAPI_CALL mSuggest(XrInstance i, const XrInteractionProfileSuggestedBinding* b) {
    (void)i;
    const char* prof = pathStr(b->interactionProfile);
    int pi = -1;
    for (int k = 0; k < NPROFILES; k++)
        if (!strcmp(prof, kProfiles[k])) pi = k;
    if (pi < 0) {
        mock_error("suggested bindings for unknown profile %s", prof);
        return XR_ERROR_PATH_UNSUPPORTED;
    }
    if (pi == 0 && !gFrameControllerExt) {
        mock_error("suggested bindings for %s without XR_VALVE_frame_controller_interaction", prof);
        return XR_ERROR_PATH_UNSUPPORTED;
    }
    LOCK();
    if (gAttached) {
        UNLOCK();
        return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
    }
    // A new suggestion for a profile replaces the old one.
    int w = 0;
    for (int k = 0; k < gBindCount; k++)
        if (gBind[k].profile != pi) gBind[w++] = gBind[k];
    gBindCount = w;
    for (uint32_t k = 0; k < b->countSuggestedBindings && gBindCount < MAXBIND; k++) {
        Bind* bd = &gBind[gBindCount++];
        bd->act = (int)((uintptr_t)b->suggestedBindings[k].action - 200);
        bd->profile = pi;
        strncpy(bd->path, pathStr(b->suggestedBindings[k].binding), sizeof(bd->path) - 1);
        bd->path[sizeof(bd->path) - 1] = 0;
    }
    gProfileSuggested[pi] = 1;
    trace("suggestBindings %s count=%u", prof, b->countSuggestedBindings);
    UNLOCK();
    return XR_SUCCESS;
}

// ---- events ----------------------------------------------------------------

#define EV_PROFILE_CHANGED 1000
static int gEventQueue[16], gEventCount = 0;
static int gQuitSent = 0;

static void pushState(int s) {
    LOCK();
    if (gEventCount < 16) gEventQueue[gEventCount++] = s;
    UNLOCK();
}

static XrResult XRAPI_CALL mAttach(XrSession s, const XrSessionActionSetsAttachInfo* a) {
    (void)s;
    (void)a;
    LOCK();
    gAttached = 1;
    gProfile = gProfileSuggested[0] ? 0 : -1;
    for (int k = 1; k < NPROFILES && gProfile < 0; k++)
        if (gProfileSuggested[k]) gProfile = k;
    trace("attachActionSets profile=%s", gProfile >= 0 ? kProfiles[gProfile] : "none");
    if (gProfile >= 0) pushState(EV_PROFILE_CHANGED);
    UNLOCK();
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mSync(XrSession s, const XrActionsSyncInfo* a) {
    (void)s;
    (void)a;
    LOCK();
    if (!gAttached) {
        UNLOCK();
        return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    }
    gSynced = 1;
    applyScript();
    UNLOCK();
    return XR_SUCCESS;
}

// The state of an action for a subaction path: 1 when it has a binding in the
// active profile.  `tab`: the live or previous input table.
static int actionState(XrAction action, XrPath sub, In* tab, double* x, double* y) {
    int ai = (int)((uintptr_t)action - 200);
    const char* subs = sub ? pathStr(sub) : "";
    size_t sl = strlen(subs);
    int bound = 0;
    *x = *y = 0;
    for (int b = 0; b < gBindCount; b++) {
        if (gBind[b].act != ai || gBind[b].profile != gProfile) continue;
        if (sl && (strncmp(gBind[b].path, subs, sl) || gBind[b].path[sl] != '/')) continue;
        bound = 1;
        double vx, vy;
        if (inValue(tab, gBind[b].path, &vx, &vy) && fabs(vx) + fabs(vy) > fabs(*x) + fabs(*y)) {
            *x = vx;
            *y = vy;
        }
    }
    return bound;
}

static XrResult stateCheck(const XrActionStateGetInfo* g, XrActionType want) {
    int ai = (int)((uintptr_t)g->action - 200);
    if (ai < 0 || ai >= gActCount) return XR_ERROR_HANDLE_INVALID;
    if (gAct[ai].type != want) {
        mock_error("action %s read with the wrong type", gAct[ai].name);
        return XR_ERROR_ACTION_TYPE_MISMATCH;
    }
    if (!gAttached) {
        mock_error("action %s read before the action sets were attached", gAct[ai].name);
        return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    }
    return XR_SUCCESS;
}

static XrResult XRAPI_CALL mGetBool(XrSession s, const XrActionStateGetInfo* g, XrActionStateBoolean* o) {
    (void)s;
    LOCK();
    XrResult r = stateCheck(g, XR_ACTION_TYPE_BOOLEAN_INPUT);
    if (r == XR_SUCCESS) {
        double x, y, px, py;
        int bound = actionState(g->action, g->subactionPath, gIn, &x, &y);
        actionState(g->action, g->subactionPath, gInPrev, &px, &py);
        o->isActive = gSynced && bound ? XR_TRUE : XR_FALSE;
        o->currentState = o->isActive && (x > 0.5 || x < -0.5) ? XR_TRUE : XR_FALSE;
        o->changedSinceLastSync = o->isActive && ((x > 0.5 || x < -0.5) != (px > 0.5 || px < -0.5)) ? XR_TRUE : XR_FALSE;
        o->lastChangeTime = 0;
    }
    UNLOCK();
    return r;
}
static XrResult XRAPI_CALL mGetFloat(XrSession s, const XrActionStateGetInfo* g, XrActionStateFloat* o) {
    (void)s;
    LOCK();
    XrResult r = stateCheck(g, XR_ACTION_TYPE_FLOAT_INPUT);
    if (r == XR_SUCCESS) {
        double x, y, px, py;
        int bound = actionState(g->action, g->subactionPath, gIn, &x, &y);
        actionState(g->action, g->subactionPath, gInPrev, &px, &py);
        o->isActive = gSynced && bound ? XR_TRUE : XR_FALSE;
        o->currentState = o->isActive ? (float)x : 0.0f;
        o->changedSinceLastSync = o->isActive && x != px ? XR_TRUE : XR_FALSE;
        o->lastChangeTime = 0;
    }
    UNLOCK();
    return r;
}
static XrResult XRAPI_CALL mGetVec2(XrSession s, const XrActionStateGetInfo* g, XrActionStateVector2f* o) {
    (void)s;
    LOCK();
    XrResult r = stateCheck(g, XR_ACTION_TYPE_VECTOR2F_INPUT);
    if (r == XR_SUCCESS) {
        double x, y, px, py;
        int bound = actionState(g->action, g->subactionPath, gIn, &x, &y);
        actionState(g->action, g->subactionPath, gInPrev, &px, &py);
        o->isActive = gSynced && bound ? XR_TRUE : XR_FALSE;
        o->currentState.x = o->isActive ? (float)x : 0.0f;
        o->currentState.y = o->isActive ? (float)y : 0.0f;
        o->changedSinceLastSync = o->isActive && (x != px || y != py) ? XR_TRUE : XR_FALSE;
        o->lastChangeTime = 0;
    }
    UNLOCK();
    return r;
}
static XrResult XRAPI_CALL mGetPoseState(XrSession s, const XrActionStateGetInfo* g, XrActionStatePose* o) {
    (void)s;
    LOCK();
    XrResult r = stateCheck(g, XR_ACTION_TYPE_POSE_INPUT);
    if (r == XR_SUCCESS) {
        double x, y;
        o->isActive = gSynced && actionState(g->action, g->subactionPath, gIn, &x, &y) ? XR_TRUE : XR_FALSE;
    }
    UNLOCK();
    return r;
}
static XrResult XRAPI_CALL mHaptic(XrSession s, const XrHapticActionInfo* h, const XrHapticBaseHeader* b) {
    (void)s;
    const XrHapticVibration* v = (const XrHapticVibration*)b;
    int ai = (int)((uintptr_t)h->action - 200);
    if (b->type == XR_TYPE_HAPTIC_VIBRATION)
        trace("haptic %s %s amp=%.2f freq=%.0f dur=%lld", ai >= 0 && ai < gActCount ? gAct[ai].name : "?", pathStr(h->subactionPath), v->amplitude,
              v->frequency, (long long)v->duration);
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mHapticStop(XrSession s, const XrHapticActionInfo* h) {
    (void)s;
    (void)h;
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mGetProfile(XrSession s, XrPath p, XrInteractionProfileState* o) {
    (void)s;
    const char* ps = pathStr(p);
    int hand = !strcmp(ps, "/user/hand/left") || !strcmp(ps, "/user/hand/right");
    if (hand && gAttached && gProfile >= 0) {
        XrPath out = XR_NULL_PATH;
        mStringToPath(0, kProfiles[gProfile], &out);
        o->interactionProfile = out;
    } else {
        o->interactionProfile = XR_NULL_PATH;
    }
    return XR_SUCCESS;
}

// ---- session, spaces -------------------------------------------------------

static XrResult XRAPI_CALL mCreateSession(XrInstance i, const XrSessionCreateInfo* ci, XrSession* out) {
    (void)i;
    for (const XrBaseInStructure* n = (const XrBaseInStructure*)ci->next; n && !gGfx; n = n->next) {
        for (unsigned t = 0; t < sizeof(kGfxTables) / sizeof(kGfxTables[0]); t++) {
            if (kGfxTables[t]->owns_binding(n->type)) {
                char desc[256];
                gGfx = kGfxTables[t];
                gGfx->describe_binding(n, desc, sizeof(desc));
                trace("createSession %s", desc);
                break;
            }
        }
    }
    if (!gGfx) {
        mock_error("createSession: no graphics binding the mock knows");
        return XR_ERROR_GRAPHICS_DEVICE_INVALID;
    }
    static int sessions = 0;
    if (sessions++ > 0) trace("createSession again (%d)", sessions);
    *out = (XrSession)(uintptr_t)2;
    pushState(XR_SESSION_STATE_IDLE);
    pushState(XR_SESSION_STATE_READY);
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mDestroySession(XrSession s) {
    (void)s;
    trace("destroySession");
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mCreateRefSpace(XrSession s, const XrReferenceSpaceCreateInfo* ci, XrSpace* out) {
    (void)s;
    int kind = SP_STAGE;
    switch (ci->referenceSpaceType) {
    case XR_REFERENCE_SPACE_TYPE_VIEW: kind = SP_VIEW; break;
    case XR_REFERENCE_SPACE_TYPE_LOCAL: kind = SP_LOCAL; break;
    default: kind = SP_STAGE; break;
    }
    *out = newSpace(kind, 0, XR_NULL_PATH, ci->poseInReferenceSpace);
    trace("createReferenceSpace %s", kSpaceNames[kind]);
    return *out ? XR_SUCCESS : XR_ERROR_LIMIT_REACHED;
}
static XrResult XRAPI_CALL mEnumRefSpaces(XrSession s, uint32_t cap, uint32_t* count, XrReferenceSpaceType* t) {
    (void)s;
    *count = 3;
    if (cap >= 3 && t) {
        t[0] = XR_REFERENCE_SPACE_TYPE_VIEW;
        t[1] = XR_REFERENCE_SPACE_TYPE_LOCAL;
        t[2] = XR_REFERENCE_SPACE_TYPE_STAGE;
    }
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mCreateActionSpace(XrSession s, const XrActionSpaceCreateInfo* ci, XrSpace* out) {
    (void)s;
    *out = newSpace(SP_ACTION, (int)((uintptr_t)ci->action - 200), ci->subactionPath, ci->poseInActionSpace);
    return *out ? XR_SUCCESS : XR_ERROR_LIMIT_REACHED;
}
static XrResult XRAPI_CALL mDestroySpace(XrSpace s) {
    (void)s;
    return XR_SUCCESS;
}

// The pose of an action's space in LOCAL space (origin where the head began):
// the script's pose for a path the action is bound to under the hand, else a
// hand at rest.
static XrPosef handPose(int act, XrPath sub) {
    const char* subs = sub ? pathStr(sub) : "";
    size_t sl = strlen(subs);
    for (int b = 0; b < gBindCount; b++) {
        if (gBind[b].act != act || gBind[b].profile != gProfile) continue;
        if (sl && (strncmp(gBind[b].path, subs, sl) || gBind[b].path[sl] != '/')) continue;
        In* in = inFind(gIn, gBind[b].path);
        if (in && in->isPose) return in->pose;
    }
    int left = sl && !strcmp(subs, "/user/hand/left");
    return (XrPosef){{0, 0, 0, 1}, {left ? -0.25f : 0.25f, -0.4f, -0.35f}};
}

static XrPosef worldOf(const Space* sp, XrTime t) {
    XrPosef w;
    switch (sp->kind) {
    case SP_LOCAL: w = (XrPosef){{0, 0, 0, 1}, {0, 1.6f, 0}}; break;  // where the head began
    case SP_VIEW: w = headAt(t); break;
    case SP_ACTION: w = pmul((XrPosef){{0, 0, 0, 1}, {0, 1.6f, 0}}, handPose(sp->act, sp->sub)); break;
    default: w = pident(); break;
    }
    return pmul(w, sp->offset);
}

static XrResult XRAPI_CALL mLocateSpace(XrSpace spc, XrSpace base, XrTime t, XrSpaceLocation* loc) {
    LOCK();
    Space *a = spaceOf(spc), *b = spaceOf(base);
    if (!a || !b) {
        UNLOCK();
        return XR_ERROR_HANDLE_INVALID;
    }
    loc->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT |
                         XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    loc->pose = pmul(pinv(worldOf(b, t)), worldOf(a, t));
    UNLOCK();
    return XR_SUCCESS;
}

// ---- views, swapchains -----------------------------------------------------

static XrResult XRAPI_CALL mEnumViewConfigViews(XrInstance i, XrSystemId s, XrViewConfigurationType t, uint32_t cap, uint32_t* count,
                                                XrViewConfigurationView* v) {
    (void)i;
    (void)s;
    (void)t;
    *count = 2;
    if (cap == 0) return XR_SUCCESS;
    for (int e = 0; e < 2; e++) {
        v[e].recommendedImageRectWidth = 1680;
        v[e].recommendedImageRectHeight = 1760;
        v[e].maxImageRectWidth = 4096;
        v[e].maxImageRectHeight = 4096;
        v[e].recommendedSwapchainSampleCount = 1;
        v[e].maxSwapchainSampleCount = 1;
    }
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mEnumFormats(XrSession s, uint32_t cap, uint32_t* count, int64_t* f) {
    (void)s;
    const int64_t* list;
    int n = gGfx->formats(&list);
    *count = (uint32_t)n;
    if (cap == 0) return XR_SUCCESS;
    for (int i = 0; i < n && i < (int)cap; i++) f[i] = list[i];
    return XR_SUCCESS;
}

#define MAXSC 256
typedef struct {
    int w, h, mips, count;
    int64_t format;
    void* images;
    int acquired, next;
    int waited;       // of the images acquired, how many were waited for
    int live;
    long releases;    // total
    int lastRel;      // index of the image released last (-1: none)
    int dumped;       // how many of gDumpFrames were dumped
} Chain;
static Chain gChains[MAXSC];
static int gChainCount = 0;

static XrResult XRAPI_CALL mCreateSwapchain(XrSession s, const XrSwapchainCreateInfo* ci, XrSwapchain* out) {
    (void)s;
    LOCK();
    if (gChainCount >= MAXSC) {
        UNLOCK();
        return XR_ERROR_LIMIT_REACHED;
    }
    const int64_t* fl;
    int nf = gGfx->formats(&fl);
    int okf = 0;
    for (int i = 0; i < nf; i++) okf |= fl[i] == ci->format;
    if (!okf) {
        UNLOCK();
        mock_error("createSwapchain with format 0x%llx, which the runtime does not offer", (long long)ci->format);
        return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
    }
    if (ci->arraySize > 1 || ci->faceCount > 1 || ci->sampleCount > 1) {
        UNLOCK();
        mock_error("createSwapchain with arraySize %u faceCount %u sampleCount %u (the mock makes plain images)", ci->arraySize, ci->faceCount,
                   ci->sampleCount);
        return XR_ERROR_FEATURE_UNSUPPORTED;
    }
    Chain* c = &gChains[gChainCount];
    memset(c, 0, sizeof(*c));
    c->w = (int)ci->width;
    c->h = (int)ci->height;
    c->mips = (int)ci->mipCount;
    c->format = ci->format;
    c->count = 3;
    c->live = 1;
    c->lastRel = -1;
    c->images = gGfx->create_images(c->format, ci->usageFlags, c->w, c->h, c->mips, c->count);
    trace("createSwapchain sc%d %dx%d mips=%d format=0x%llx usage=0x%llx", gChainCount, c->w, c->h, c->mips, (long long)c->format,
          (unsigned long long)ci->usageFlags);
    if (ci->mipCount > 1) {
        // SteamVR on the Frame allocates a Vulkan swapchain image's memory for
        // level 0 only: levels below it are written past the image.
        mock_error("createSwapchain sc%d with %u mip levels: SteamVR allocates one", gChainCount, ci->mipCount);
    }
    *out = (XrSwapchain)(uintptr_t)(1000 + gChainCount);
    gChainCount++;
    UNLOCK();
    return XR_SUCCESS;
}
static Chain* chainOf(XrSwapchain sc) {
    uintptr_t i = (uintptr_t)sc;
    if (i < 1000 || i - 1000 >= (uintptr_t)gChainCount) return NULL;
    return &gChains[i - 1000];
}
static int chainId(XrSwapchain sc) { return (int)((uintptr_t)sc - 1000); }
static XrResult XRAPI_CALL mDestroySwapchain(XrSwapchain sc) {
    LOCK();
    Chain* c = chainOf(sc);
    if (c) {
        if (c->acquired) mock_error("destroySwapchain sc%d with %d images still acquired", chainId(sc), c->acquired);
        c->live = 0;
        trace("destroySwapchain sc%d releases=%ld", chainId(sc), c->releases);
    }
    UNLOCK();
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mEnumSwapImages(XrSwapchain sc, uint32_t cap, uint32_t* count, XrSwapchainImageBaseHeader* im) {
    Chain* c = chainOf(sc);
    if (!c) return XR_ERROR_HANDLE_INVALID;
    *count = (uint32_t)c->count;
    if (cap == 0) return XR_SUCCESS;
    return gGfx->fill_images(c->images, c->count < (int)cap ? c->count : (int)cap, im);
}

// ---- per-second summary ----------------------------------------------------

static struct {
    int waits, begins, ends;
    int layersMin, layersMax, quads, projs, cyls;
    int rel[MAXSC];
    int busy;
} gSec;
static int gSecIdx = 0;
static long gTotalWaits = 0, gTotalEnds = 0;

static void secReset(void) {
    memset(&gSec, 0, sizeof(gSec));
    gSec.layersMin = 1 << 20;
}

static void secEmit(void) {
    char rel[1024] = "";
    for (int i = 0; i < gChainCount; i++) {
        if (!gChains[i].live) continue;
        char t[32];
        snprintf(t, sizeof(t), " sc%d=%d", i, gSec.rel[i]);
        if (strlen(rel) + strlen(t) < sizeof(rel)) strcat(rel, t);
    }
    trace("summary t=%ds waits=%d begins=%d ends=%d layers=%d..%d quads=%d proj=%d cyl=%d releases:%s busy=%d errors=%d", gSecIdx, gSec.waits,
          gSec.begins, gSec.ends, gSec.ends ? gSec.layersMin : 0, gSec.layersMax, gSec.quads, gSec.projs, gSec.cyls, rel, gSec.busy, gErrors);
    secReset();
}

// Called with the lock held, from the frame and release calls.
static void secTick(void) {
    if (!gT0) return;
    int idx = (int)((nowNs() - gT0) / 1000000000ll);
    if (idx > gSecIdx) {
        gSecIdx = idx;
        secEmit();
    }
}

// ---- image dumps -----------------------------------------------------------

static const char* gDumpDir = NULL;
static int gDumpFrame = -1;  // -1: the last image of each swapchain, at the end
#define MAXDUMPS 64
static int gDumpFrames[MAXDUMPS];  // ascending; gDumpFrame = gDumpFrames[0]
static int gDumpCount = 0;
// MOCK_XR_DUMP_SHOWN=t1,t2,...: at the first xrEndFrame at or after each time (seconds
// since the session began), the image each submitted quad shows (its last released
// one), as sc<id>_shown_f<frame>.png.
static double gShownTimes[MAXDUMPS];
static int gShownCount = 0, gShownNext = 0;

static uint32_t crcTab[256];
static uint32_t crc32(uint32_t crc, const uint8_t* p, size_t n) {
    if (!crcTab[1]) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
            crcTab[i] = c;
        }
    }
    crc = ~crc;
    while (n--) crc = crcTab[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return ~crc;
}
static void be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
static void chunk(FILE* f, const char* type, const uint8_t* data, uint32_t n) {
    uint8_t h[8];
    be32(h, n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n) fwrite(data, 1, n, f);
    uint32_t c = crc32(0, (const uint8_t*)type, 4);
    c = crc32(c, data, n);
    uint8_t t[4];
    be32(t, c);
    fwrite(t, 1, 4, f);
}
// RGBA8 as a PNG with stored (uncompressed) deflate blocks: no zlib needed.
static int writePng(const char* path, const uint8_t* rgba, int w, int h) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    size_t row = (size_t)w * 4 + 1, raw = row * (size_t)h;
    uint8_t* z = (uint8_t*)malloc(raw + raw / 65535 * 5 + 64);
    if (!z) {
        fclose(f);
        return -1;
    }
    size_t zn = 0;
    z[zn++] = 0x78;
    z[zn++] = 0x01;
    uint32_t a = 1, b = 0;
    size_t pos = 0;
    while (pos < raw || raw == 0) {
        size_t n = raw - pos > 65535 ? 65535 : raw - pos;
        z[zn++] = pos + n >= raw ? 1 : 0;
        z[zn++] = (uint8_t)n;
        z[zn++] = (uint8_t)(n >> 8);
        z[zn++] = (uint8_t)~n;
        z[zn++] = (uint8_t)(~n >> 8);
        for (size_t i = 0; i < n; i++) {
            size_t k = pos + i;
            uint8_t v = (k % row == 0) ? 0 : rgba[(k / row) * (size_t)w * 4 + (k % row) - 1];
            z[zn++] = v;
            a = (a + v) % 65521;
            b = (b + a) % 65521;
        }
        pos += n;
        if (raw == 0) break;
    }
    be32(z + zn, (b << 16) | a);
    zn += 4;
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13];
    be32(ihdr, (uint32_t)w);
    be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;
    ihdr[9] = 6;
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (uint32_t)zn);
    chunk(f, "IEND", NULL, 0);
    free(z);
    return fclose(f);
}

static void dumpImage(int id, int index, const char* tag) {
    Chain* c = &gChains[id];
    if (!gDumpDir || !gGfx->read_image || index < 0) return;
    uint8_t* px = (uint8_t*)malloc((size_t)c->w * (size_t)c->h * 4);
    if (!px) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/sc%d_%s.png", gDumpDir, id, tag);
    if (gGfx->read_image(c->images, index, c->w, c->h, c->format, px) == 0 && writePng(path, px, c->w, c->h) == 0)
        trace("dump sc%d image %d %dx%d -> %s", id, index, c->w, c->h, path);
    else
        trace("dump sc%d not written (format 0x%llx)", id, (long long)c->format);
    free(px);
}

// ---- acquire / wait / release ----------------------------------------------

static int gReleases = 0, gBusyReleases = 0;
static double gBusyMaxMs = 0.0;
static int gWaitHist[5];
static int gPoseAge[6];  // display time minus locate time in refreshes: <0, 0, 1, 2, 3, more
static int gPoseSets = 0, gPoseSameTime = 0, gPoseEarlier = 0, gPoseLater = 0, gPoseSplit = 0, gPoseUnknown = 0;
static void checkPoses(const XrCompositionLayerProjection* p, XrTime displayTime);

static XrResult XRAPI_CALL mAcquire(XrSwapchain sc, const XrSwapchainImageAcquireInfo* ai, uint32_t* idx) {
    (void)ai;
    LOCK();
    Chain* c = chainOf(sc);
    if (!c) {
        UNLOCK();
        return XR_ERROR_HANDLE_INVALID;
    }
    if (c->acquired >= c->count) {
        mock_error("acquire sc%d with all images acquired", chainId(sc));
        UNLOCK();
        return XR_ERROR_CALL_ORDER_INVALID;
    }
    *idx = (uint32_t)c->next;
    c->next = (c->next + 1) % c->count;
    c->acquired++;
    trace("acquire sc%d idx=%u", chainId(sc), *idx);
    UNLOCK();
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mWaitImage(XrSwapchain sc, const XrSwapchainImageWaitInfo* wi) {
    (void)wi;
    LOCK();
    Chain* c = chainOf(sc);
    if (!c) {
        UNLOCK();
        return XR_ERROR_HANDLE_INVALID;
    }
    if (c->waited >= c->acquired) {
        mock_error("wait sc%d without an image acquired", chainId(sc));
        UNLOCK();
        return XR_ERROR_CALL_ORDER_INVALID;
    }
    // OpenXR: one image of a swapchain waited for at a time (SteamVR returns
    // XR_ERROR_CALL_ORDER_INVALID, and GalaxyFrame aborted on it in round 3).
    if (c->waited > 0) {
        mock_error("wait sc%d with an image already waited for and not released", chainId(sc));
        UNLOCK();
        return XR_ERROR_CALL_ORDER_INVALID;
    }
    c->waited++;
    UNLOCK();
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mRelease(XrSwapchain sc, const XrSwapchainImageReleaseInfo* ri) {
    (void)ri;
    LOCK();
    Chain* c = chainOf(sc);
    if (!c) {
        UNLOCK();
        return XR_ERROR_HANDLE_INVALID;
    }
    if (c->acquired <= 0) {
        mock_error("release sc%d without acquire", chainId(sc));
        UNLOCK();
        return XR_ERROR_CALL_ORDER_INVALID;
    }
    if (c->waited <= 0) {
        mock_error("release sc%d of an image not waited for", chainId(sc));
        UNLOCK();
        return XR_ERROR_CALL_ORDER_INVALID;
    }
    // Opt-in (MOCK_XR_BUSY_MS): waits for the app's queue to go idle.
    static int busyCheck = -1;
    if (busyCheck < 0) busyCheck = getenv("MOCK_XR_BUSY_MS") != NULL;
    if (busyCheck && gGfx && gGfx->release_wait_ms) {
        static double limit = -1.0;
        if (limit < 0.0) limit = getenv("MOCK_XR_BUSY_MS") ? atof(getenv("MOCK_XR_BUSY_MS")) : 4.0;
        double ms = gGfx->release_wait_ms();
        gReleases++;
        gWaitHist[ms < 0.5 ? 0 : ms < 1.0 ? 1 : ms < 2.0 ? 2 : ms < 4.0 ? 3 : 4]++;
        if (ms > limit) {
            gBusyReleases++;
            gSec.busy++;
            if (ms > gBusyMaxMs) gBusyMaxMs = ms;
            mock_error("release sc%d before the GPU finished (%.1f ms)", chainId(sc), ms);
        }
    }
    int idx = ((c->next - c->acquired) % c->count + c->count) % c->count;
    if (gDumpDir && gDumpFrame >= 0 && c->dumped < gDumpCount && gFrame >= gDumpFrames[c->dumped]) {
        char tag[32];
        snprintf(tag, sizeof(tag), "f%d", gFrame);
        dumpImage(chainId(sc), idx, tag);
        while (c->dumped < gDumpCount && gFrame >= gDumpFrames[c->dumped]) c->dumped++;
    }
    c->acquired--;
    c->waited--;
    c->releases++;
    c->lastRel = idx;
    gSec.rel[chainId(sc)]++;
    trace("release sc%d", chainId(sc));
    secTick();
    UNLOCK();
    return XR_SUCCESS;
}

// ---- session, frames -------------------------------------------------------

static XrResult XRAPI_CALL mBeginSession(XrSession s, const XrSessionBeginInfo* bi) {
    (void)s;
    (void)bi;
    LOCK();
    secReset();
    if (gSessionsBegun++ == 0) {
        gT0 = nowNs();
        const char* secs = getenv("MOCK_XR_SECONDS");
        double limit = secs ? atof(secs) : 20.0;  // 0: no limit
        gEndAt = limit > 0 ? gT0 + (int64_t)(limit * 1e9) : 0;
    } else {
        gResync = 1;
    }
    pushState(XR_SESSION_STATE_SYNCHRONIZED);
    pushState(XR_SESSION_STATE_VISIBLE);
    pushState(XR_SESSION_STATE_FOCUSED);
    trace("beginSession %d at %.2f s", gSessionsBegun, (double)(nowNs() - gT0) / 1e9);
    UNLOCK();
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mEndSession(XrSession s) {
    (void)s;
    LOCK();
    secEmit();
    for (int i = 0; i < gChainCount; i++) {
        Chain* c = &gChains[i];
        if (gDumpDir && gDumpFrame < 0 && c->releases > 0) dumpImage(i, c->lastRel, "last");
        if (c->live) trace("total sc%d %dx%d releases=%ld", i, c->w, c->h, c->releases);
    }
    trace("total waits=%ld ends=%ld errors=%d", gTotalWaits, gTotalEnds, gErrors);
    fprintf(stderr,
            "xrmock: summary: %d errors; %d releases checked for finished GPU work, %d before it finished (worst %.1f ms); "
            "%d projection layers: eyes from one locate for the display time submitted %d, for an earlier one %d, a later one %d; eyes from "
            "different locates %d, poses not from a locate %d\n",
            gErrors, gReleases, gBusyReleases, gBusyMaxMs, gPoseSets, gPoseSameTime, gPoseEarlier, gPoseLater, gPoseSplit, gPoseUnknown);
    if (gPoseSets)
        fprintf(stderr, "xrmock: projection layers by refreshes from the locate to the display time: later %d, 0: %d, 1: %d, 2: %d, 3: %d, more %d\n",
                gPoseAge[0], gPoseAge[1], gPoseAge[2], gPoseAge[3], gPoseAge[4], gPoseAge[5]);
    if (gReleases) {
        fprintf(stderr, "xrmock: queue drain time at the releases: <0.5 ms %d, 0.5-1 %d, 1-2 %d, 2-4 %d, >4 %d\n", gWaitHist[0], gWaitHist[1], gWaitHist[2],
                gWaitHist[3], gWaitHist[4]);
    }
    fprintf(stderr, "xrmock: %ld frames waited, %ld submitted, %d errors\n", gTotalWaits, gTotalEnds, gErrors);
    pushState(XR_SESSION_STATE_EXITING);
    trace("endSession");
    UNLOCK();
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mRequestExit(XrSession s) {
    (void)s;
    pushState(XR_SESSION_STATE_STOPPING);
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mPollEvent(XrInstance i, XrEventDataBuffer* ev) {
    (void)i;
    LOCK();
    if (gEventCount == 0) {
        UNLOCK();
        return XR_EVENT_UNAVAILABLE;
    }
    int st = gEventQueue[0];
    memmove(gEventQueue, gEventQueue + 1, (size_t)(--gEventCount) * sizeof(int));
    UNLOCK();
    if (st == EV_PROFILE_CHANGED) {
        XrEventDataInteractionProfileChanged* e = (XrEventDataInteractionProfileChanged*)ev;
        e->type = XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;
        e->next = NULL;
        e->session = (XrSession)(uintptr_t)2;
        trace("event interactionProfileChanged");
        return XR_SUCCESS;
    }
    XrEventDataSessionStateChanged* e = (XrEventDataSessionStateChanged*)ev;
    e->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
    e->next = NULL;
    e->session = (XrSession)(uintptr_t)2;
    e->state = (XrSessionState)st;
    e->time = 0;
    trace("event sessionState=%d", st);
    return XR_SUCCESS;
}

static int gWaits = 0;     // refreshes handed out
static int gWaitedFrames;  // xrWaitFrame calls not yet begun
static int gBegun;         // xrBeginFrame calls not yet ended
static XrResult XRAPI_CALL mWaitFrame(XrSession s, const XrFrameWaitInfo* wi, XrFrameState* fs) {
    (void)s;
    (void)wi;
    int64_t now = nowNs();
    // MOCK_XR_SKIP=1: a frame loop that falls behind loses refreshes, as on a
    // headset (the next frame waits for the next refresh to come).
    static int skip = -1;
    if (skip < 0) skip = getenv("MOCK_XR_SKIP") != NULL;
    LOCK();
    if (gResync || (skip && gT0 && now > gT0 + (int64_t)gWaits * gPeriod + gPeriod / 2)) {
        gResync = 0;
        gWaits = (int)((now - gT0) / gPeriod) + 1;
    }
    int64_t wake = gT0 + (int64_t)gWaits * gPeriod;
    gWaits++;
    UNLOCK();
    if (wake > now) {
        struct timespec ts = {(time_t)((wake - now) / 1000000000ll), (long)((wake - now) % 1000000000ll)};
        nanosleep(&ts, NULL);
    }
    LOCK();
    fs->predictedDisplayTime = wake + 2 * gPeriod;
    fs->predictedDisplayPeriod = gPeriod;
    fs->shouldRender = XR_TRUE;
    if (gEndAt && nowNs() > gEndAt && !gQuitSent) {
        gQuitSent = 1;
        pushState(XR_SESSION_STATE_STOPPING);
    }
    gFrame++;
    gWaitedFrames++;
    gTotalWaits++;
    gSec.waits++;
    trace("waitFrame");
    secTick();
    UNLOCK();
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mBeginFrame(XrSession s, const XrFrameBeginInfo* bi) {
    (void)s;
    (void)bi;
    LOCK();
    if (gWaitedFrames <= 0) {
        mock_error("beginFrame without a waitFrame");
        UNLOCK();
        return XR_ERROR_CALL_ORDER_INVALID;
    }
    gWaitedFrames--;
    gSec.begins++;
    XrResult r = XR_SUCCESS;
    if (gBegun) {
        trace("beginFrame discards the frame begun before it");
        r = XR_FRAME_DISCARDED;
    } else {
        trace("beginFrame");
    }
    gBegun = 1;
    UNLOCK();
    return r;
}
static XrResult XRAPI_CALL mEndFrame(XrSession s, const XrFrameEndInfo* ei) {
    (void)s;
    char buf[2048] = "";
    // Opt-in (MOCK_XR_END_GPU=1): the call lasts until the app's queue is idle.
    static int endGpu = -1;
    if (endGpu < 0) endGpu = getenv("MOCK_XR_END_GPU") && getenv("MOCK_XR_END_GPU")[0] == '1';
    if (endGpu && gGfx && gGfx->release_wait_ms) gGfx->release_wait_ms();
    LOCK();
    if (!gBegun) {
        mock_error("endFrame without a beginFrame");
        UNLOCK();
        return XR_ERROR_CALL_ORDER_INVALID;
    }
    gBegun = 0;
    int quads = 0, projs = 0, cyls = 0;
    int shown[32];
    int nshown = 0;
    XrResult res = XR_SUCCESS;
    for (uint32_t i = 0; i < ei->layerCount; i++) {
        const XrCompositionLayerBaseHeader* l = ei->layers[i];
        char t[320];
        if (l->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
            const XrCompositionLayerProjection* p = (const XrCompositionLayerProjection*)l;
            projs++;
            checkPoses(p, ei->displayTime);
            snprintf(t, sizeof(t), " proj[flags=%llx", (unsigned long long)p->layerFlags);
            strcat(buf, t);
            for (uint32_t v = 0; v < p->viewCount; v++) {
                const XrCompositionLayerProjectionView* pv = &p->views[v];
                int warp = 0;
                for (const XrBaseInStructure* n = (const XrBaseInStructure*)pv->next; n; n = n->next) warp |= n->type == XR_TYPE_COMPOSITION_LAYER_SPACE_WARP_INFO_FB;
                snprintf(t, sizeof(t), " sc%d:%dx%d%s", chainId(pv->subImage.swapchain), pv->subImage.imageRect.extent.width,
                         pv->subImage.imageRect.extent.height, warp ? "+warp" : "");
                strcat(buf, t);
            }
            strcat(buf, "]");
        } else if (l->type == XR_TYPE_COMPOSITION_LAYER_QUAD) {
            const XrCompositionLayerQuad* q = (const XrCompositionLayerQuad*)l;
            quads++;
            if (nshown < 32 && chainOf(q->subImage.swapchain)) {
                int id = chainId(q->subImage.swapchain), dup = 0;
                for (int k = 0; k < nshown; k++) dup |= shown[k] == id;
                if (!dup) shown[nshown++] = id;
            }
            Chain* c = chainOf(q->subImage.swapchain);
            Space* sp = spaceOf(q->space);
            if (!c) {
                mock_error("endFrame quad layer %u with an unknown swapchain", i);
                res = XR_ERROR_LAYER_INVALID;
                continue;
            }
            if (c->releases == 0) {
                mock_error("endFrame quad layer %u submits sc%d, which has no image released", i, chainId(q->subImage.swapchain));
                res = XR_ERROR_LAYER_INVALID;
            }
            const XrRect2Di* r = &q->subImage.imageRect;
            if (r->offset.x < 0 || r->offset.y < 0 || r->offset.x + r->extent.width > c->w || r->offset.y + r->extent.height > c->h) {
                mock_error("endFrame quad layer %u: image rect %d,%d %dx%d outside sc%d (%dx%d)", i, r->offset.x, r->offset.y, r->extent.width,
                           r->extent.height, chainId(q->subImage.swapchain), c->w, c->h);
                res = XR_ERROR_VALIDATION_FAILURE;
            }
            if (q->subImage.imageArrayIndex != 0) mock_error("endFrame quad layer %u with array index %u", i, q->subImage.imageArrayIndex);
            if (!sp) {
                mock_error("endFrame quad layer %u with an unknown space", i);
                res = XR_ERROR_LAYER_INVALID;
            }
            snprintf(t, sizeof(t), " quad[sc%d:%dx%d at %d,%d eye=%d sp=%s size=%.2fx%.2f", chainId(q->subImage.swapchain), r->extent.width, r->extent.height,
                     r->offset.x, r->offset.y, (int)q->eyeVisibility, sp ? kSpaceNames[sp->kind] : "?", q->size.width, q->size.height);
            strcat(buf, t);
            static int poseTrace = -1;
            if (poseTrace < 0) poseTrace = getenv("MOCK_XR_TRACE_POSE") != NULL;
            if (poseTrace) {
                snprintf(t, sizeof(t), " pos=%.2f,%.2f,%.2f q=%.2f,%.2f,%.2f,%.2f", q->pose.position.x, q->pose.position.y, q->pose.position.z,
                         q->pose.orientation.x, q->pose.orientation.y, q->pose.orientation.z, q->pose.orientation.w);
                strcat(buf, t);
            }
            strcat(buf, "]");
        } else if (l->type == XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR) {
            // XR_KHR_composition_layer_cylinder: the same checks as a quad, and its shape logged.
            const XrCompositionLayerCylinderKHR* c = (const XrCompositionLayerCylinderKHR*)l;
            cyls++;
            if (!gCylinderExt) mock_error("endFrame cylinder layer %u without XR_KHR_composition_layer_cylinder", i);
            Chain* ch = chainOf(c->subImage.swapchain);
            Space* sp = spaceOf(c->space);
            if (!ch || !sp) {
                mock_error("endFrame cylinder layer %u with an unknown swapchain or space", i);
                res = XR_ERROR_LAYER_INVALID;
                continue;
            }
            if (nshown < 32) {
                int id = chainId(c->subImage.swapchain), dup = 0;
                for (int k = 0; k < nshown; k++) dup |= shown[k] == id;
                if (!dup) shown[nshown++] = id;
            }
            if (ch->releases == 0) {
                mock_error("endFrame cylinder layer %u submits sc%d, which has no image released", i, chainId(c->subImage.swapchain));
                res = XR_ERROR_LAYER_INVALID;
            }
            const XrRect2Di* r = &c->subImage.imageRect;
            if (r->offset.x < 0 || r->offset.y < 0 || r->offset.x + r->extent.width > ch->w || r->offset.y + r->extent.height > ch->h) {
                mock_error("endFrame cylinder layer %u: image rect outside sc%d", i, chainId(c->subImage.swapchain));
                res = XR_ERROR_VALIDATION_FAILURE;
            }
            if (!(c->radius >= 0.0f) || !(c->centralAngle > 0.0f) || !(c->centralAngle < 6.2831853f) || !(c->aspectRatio > 0.0f)) {
                mock_error("endFrame cylinder layer %u: radius %.3f, central angle %.3f, aspect %.3f out of range", i, c->radius, c->centralAngle,
                           c->aspectRatio);
                res = XR_ERROR_VALIDATION_FAILURE;
            }
            // The arc's width and height in metres, as a quad's size.
            const float w = c->radius * c->centralAngle, h = c->aspectRatio > 0.0f ? w / c->aspectRatio : 0.0f;
            snprintf(t, sizeof(t), " cyl[sc%d:%dx%d eye=%d sp=%s r=%.2f angle=%.1f aspect=%.3f arc=%.2fx%.2f", chainId(c->subImage.swapchain),
                     r->extent.width, r->extent.height, (int)c->eyeVisibility, kSpaceNames[sp->kind], c->radius, c->centralAngle * 57.2957795f,
                     c->aspectRatio, w, h);
            strcat(buf, t);
            static int poseTraceC = -1;
            if (poseTraceC < 0) poseTraceC = getenv("MOCK_XR_TRACE_POSE") != NULL;
            if (poseTraceC) {
                snprintf(t, sizeof(t), " pos=%.2f,%.2f,%.2f q=%.2f,%.2f,%.2f,%.2f", c->pose.position.x, c->pose.position.y, c->pose.position.z,
                         c->pose.orientation.x, c->pose.orientation.y, c->pose.orientation.z, c->pose.orientation.w);
                strcat(buf, t);
            }
            strcat(buf, "]");
        } else {
            snprintf(t, sizeof(t), " layer%d", (int)l->type);
            strcat(buf, t);
        }
        if (strlen(buf) > sizeof(buf) - 400) break;
    }
    gTotalEnds++;
    gSec.ends++;
    gSec.quads += quads;
    gSec.projs += projs;
    gSec.cyls += cyls;
    if ((int)ei->layerCount < gSec.layersMin) gSec.layersMin = (int)ei->layerCount;
    if ((int)ei->layerCount > gSec.layersMax) gSec.layersMax = (int)ei->layerCount;
    trace("endFrame layers=%u%s", ei->layerCount, buf);
    const double since = (nowNs() - gT0) * 1e-9;
    if (gDumpDir && gShownNext < gShownCount && since >= gShownTimes[gShownNext]) {
        trace("shown f%d t=%.2f: %d quad swapchains", gFrame, since, nshown);
        for (int k = 0; k < nshown; k++) {
            char tag[32];
            snprintf(tag, sizeof(tag), "shown_f%d", gFrame);
            dumpImage(shown[k], gChains[shown[k]].lastRel, tag);
        }
        while (gShownNext < gShownCount && since >= gShownTimes[gShownNext]) gShownNext++;
    }
    secTick();
    UNLOCK();
    return res;
}

// The head turns slowly from side to side, so views located for different
// times differ.  The last locates are kept for the pose check in mEndFrame.
#define LOCATES 32
static struct {
    XrTime time;
    XrPosef pose[2];
} gLocates[LOCATES];
static int gLocateNext = 0;
static XrResult XRAPI_CALL mLocateViews(XrSession s, const XrViewLocateInfo* li, XrViewState* vs, uint32_t cap, uint32_t* count, XrView* v) {
    (void)s;
    vs->viewStateFlags = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_TRACKED_BIT |
                         XR_VIEW_STATE_POSITION_TRACKED_BIT;
    *count = 2;
    if (cap == 0) return XR_SUCCESS;
    LOCK();
    Space* base = spaceOf(li->space);
    XrPosef inv = base ? pinv(worldOf(base, li->displayTime)) : pident();
    XrPosef head = headAt(li->displayTime);
    for (uint32_t e = 0; e < 2 && e < cap; e++) {
        XrPosef eye = pmul(head, (XrPosef){{0, 0, 0, 1}, {e ? 0.032f : -0.032f, 0, 0}});
        v[e].pose = pmul(inv, eye);
        v[e].fov = (XrFovf){-0.84f, 0.84f, 0.84f, -0.84f};
        gLocates[gLocateNext].pose[e] = v[e].pose;
    }
    gLocates[gLocateNext].time = li->displayTime;
    gLocateNext = (gLocateNext + 1) % LOCATES;
    UNLOCK();
    return XR_SUCCESS;
}

static int samePose(const XrPosef* a, const XrPosef* b) { return !memcmp(a, b, sizeof(XrPosef)); }

// Both eyes of a projection layer: from one locate, and for which time.
static void checkPoses(const XrCompositionLayerProjection* p, XrTime displayTime) {
    if (p->viewCount != 2) return;
    gPoseSets++;
    int left = -1, both = -1;
    for (int i = 0; i < LOCATES; i++) {
        if (!gLocates[i].time) continue;
        if (samePose(&gLocates[i].pose[0], &p->views[0].pose)) {
            left = i;
            if (samePose(&gLocates[i].pose[1], &p->views[1].pose)) both = i;
        }
    }
    if (both < 0) {
        if (left >= 0) {
            gPoseSplit++;
            mock_error("projection layer eyes from different locates");
        } else {
            gPoseUnknown++;
        }
        return;
    }
    XrTime t = gLocates[both].time;
    if (t == displayTime) gPoseSameTime++;
    else if (t < displayTime) gPoseEarlier++;
    else gPoseLater++;
    long k = lround((double)(displayTime - t) / (double)gPeriod);
    gPoseAge[k < 0 ? 0 : k > 3 ? 5 : k + 1]++;
}

static XrResult XRAPI_CALL mReqRefresh(XrSession s, float hz) {
    (void)s;
    trace("requestRefreshRate %.0f", hz);
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mEnumRefresh(XrSession s, uint32_t cap, uint32_t* count, float* r) {
    (void)s;
    *count = 3;
    if (cap >= 3) {
        r[0] = 72;
        r[1] = 90;
        r[2] = 120;
    }
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mGetRefresh(XrSession s, float* hz) {
    (void)s;
    *hz = (float)(1e9 / (double)gPeriod);
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL mPerfLevel(XrSession s, XrPerfSettingsDomainEXT d, XrPerfSettingsLevelEXT l) {
    (void)s;
    (void)d;
    (void)l;
    return XR_SUCCESS;
}

// ---- entry points ----------------------------------------------------------

typedef struct {
    const char* name;
    PFN_xrVoidFunction fn;
} Entry;
#define E(n, f) {n, (PFN_xrVoidFunction)f}
static const Entry kTable[] = {
    E("xrEnumerateInstanceExtensionProperties", mEnumExt), E("xrCreateInstance", mCreateInstance), E("xrDestroyInstance", mDestroyInstance),
    E("xrGetInstanceProperties", mGetInstanceProps), E("xrResultToString", mResultToString), E("xrGetSystem", mGetSystem),
    E("xrGetSystemProperties", mGetSystemProps), E("xrEnumerateViewConfigurations", mEnumViewConfigs),
    E("xrGetViewConfigurationProperties", mGetViewConfigProps), E("xrEnumerateEnvironmentBlendModes", mEnumBlend), E("xrCreateSession", mCreateSession),
    E("xrDestroySession", mDestroySession), E("xrCreateReferenceSpace", mCreateRefSpace), E("xrEnumerateReferenceSpaces", mEnumRefSpaces),
    E("xrCreateActionSpace", mCreateActionSpace), E("xrDestroySpace", mDestroySpace), E("xrLocateSpace", mLocateSpace),
    E("xrEnumerateViewConfigurationViews", mEnumViewConfigViews), E("xrEnumerateSwapchainFormats", mEnumFormats), E("xrCreateSwapchain", mCreateSwapchain),
    E("xrDestroySwapchain", mDestroySwapchain), E("xrEnumerateSwapchainImages", mEnumSwapImages), E("xrAcquireSwapchainImage", mAcquire),
    E("xrWaitSwapchainImage", mWaitImage), E("xrReleaseSwapchainImage", mRelease), E("xrBeginSession", mBeginSession), E("xrEndSession", mEndSession),
    E("xrRequestExitSession", mRequestExit), E("xrPollEvent", mPollEvent), E("xrWaitFrame", mWaitFrame), E("xrBeginFrame", mBeginFrame),
    E("xrEndFrame", mEndFrame), E("xrLocateViews", mLocateViews), E("xrStringToPath", mStringToPath), E("xrPathToString", mPathToString),
    E("xrCreateActionSet", mCreateActionSet), E("xrDestroyActionSet", mDestroyActionSet), E("xrCreateAction", mCreateAction),
    E("xrDestroyAction", mDestroyAction), E("xrSuggestInteractionProfileBindings", mSuggest), E("xrAttachSessionActionSets", mAttach),
    E("xrSyncActions", mSync), E("xrGetActionStateBoolean", mGetBool), E("xrGetActionStateFloat", mGetFloat), E("xrGetActionStateVector2f", mGetVec2),
    E("xrGetActionStatePose", mGetPoseState), E("xrApplyHapticFeedback", mHaptic), E("xrStopHapticFeedback", mHapticStop),
    E("xrGetCurrentInteractionProfile", mGetProfile), E("xrRequestDisplayRefreshRateFB", mReqRefresh),
    E("xrEnumerateDisplayRefreshRatesFB", mEnumRefresh), E("xrGetDisplayRefreshRateFB", mGetRefresh),
    E("xrPerfSettingsSetPerformanceLevelEXT", mPerfLevel),
};

static void init(void) {
    static int done;
    if (done) return;
    done = 1;
    const char* tp = getenv("MOCK_XR_TRACE");
    if (tp) gTrace = fopen(tp, "w");
    const char* hz = getenv("MOCK_XR_HZ");
    if (hz) gPeriod = (int64_t)(1e9 / atof(hz));
    const char* sway = getenv("MOCK_XR_SWAY");
    if (sway && atoi(sway) == 0) gSway = 0;
    gDumpDir = getenv("MOCK_XR_DUMP_DIR");
    if (gDumpDir) {
        mkdir(gDumpDir, 0755);
        const char* df = getenv("MOCK_XR_DUMP_FRAME");
        if (df && strcmp(df, "last")) {
            char* copy = strdup(df);
            for (char* tok = strtok(copy, ","); tok && gDumpCount < MAXDUMPS; tok = strtok(NULL, ",")) {
                int n = atoi(tok);
                if (gDumpCount == 0 || n > gDumpFrames[gDumpCount - 1]) gDumpFrames[gDumpCount++] = n;
            }
            free(copy);
            if (gDumpCount > 0) gDumpFrame = gDumpFrames[0];
        }
    }
    const char* shownList = getenv("MOCK_XR_DUMP_SHOWN");
    if (gDumpDir && shownList) {
        char* copy = strdup(shownList);
        for (char* tok = strtok(copy, ","); tok && gShownCount < MAXDUMPS; tok = strtok(NULL, ",")) {
            double t = atof(tok);
            if (gShownCount == 0 || t > gShownTimes[gShownCount - 1]) gShownTimes[gShownCount++] = t;
        }
        free(copy);
    }
    const char* script = getenv("MOCK_XR_INPUT_SCRIPT");
    if (script) loadScript(script);
    secReset();
    const char* list = getenv("MOCK_XR_EXTENSIONS");
    if (list) {
        char* copy = strdup(list);
        for (char* tok = strtok(copy, ","); tok && gExtCount < 64; tok = strtok(NULL, ",")) strcpy(gExtNames[gExtCount++], tok);
        free(copy);
    } else {
        const char* exts[] = {"XR_KHR_vulkan_enable2", "XR_FB_display_refresh_rate", "XR_EXT_performance_settings",
                              "XR_VALVE_frame_controller_interaction", "XR_KHR_composition_layer_depth", "XR_EXT_eye_gaze_interaction",
                              "XR_KHR_composition_layer_cylinder"};
        for (unsigned i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) strcpy(gExtNames[gExtCount++], exts[i]);
        if (getenv("MOCK_XR_SPACEWARP")) strcpy(gExtNames[gExtCount++], "XR_FB_space_warp");
    }
}

static XRAPI_ATTR XrResult XRAPI_CALL mockGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* fn) {
    (void)instance;
    init();
    for (unsigned i = 0; i < sizeof(kTable) / sizeof(kTable[0]); i++) {
        if (!strcmp(kTable[i].name, name)) {
            *fn = kTable[i].fn;
            return XR_SUCCESS;
        }
    }
    // The graphics extension's own functions: the app asks for them before
    // the session exists, so any table may answer.
    for (unsigned t = 0; t < sizeof(kGfxTables) / sizeof(kGfxTables[0]); t++) {
        PFN_xrVoidFunction f = kGfxTables[t]->get_proc(name);
        if (f) {
            *fn = f;
            return XR_SUCCESS;
        }
    }
    *fn = NULL;
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

__attribute__((visibility("default"))) XrResult XRAPI_CALL xrNegotiateLoaderRuntimeInterface(const XrNegotiateLoaderInfo* li,
                                                                                            XrNegotiateRuntimeRequest* rr) {
    (void)li;
    init();
    rr->runtimeInterfaceVersion = 1;
    rr->runtimeApiVersion = XR_CURRENT_API_VERSION;
    rr->getInstanceProcAddr = mockGetInstanceProcAddr;
    return XR_SUCCESS;
}
