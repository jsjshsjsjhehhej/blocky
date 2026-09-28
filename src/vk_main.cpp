
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "stb_image.h"
#include "render/WindowIcon.h"

static constexpr float kStudTile = 4.f;
static int g_w = 1280, g_h = 720;

struct Camera {
    glm::vec3 pos{ 0.f, 12.f, 28.f };
    float yaw = -90.f, pitch = -18.f, fov = 70.f;
    glm::vec3 front() const {
        glm::vec3 f;
        f.x = cosf(glm::radians(yaw)) * cosf(glm::radians(pitch));
        f.y = sinf(glm::radians(pitch));
        f.z = sinf(glm::radians(yaw)) * cosf(glm::radians(pitch));
        return glm::normalize(f);
    }
    glm::mat4 view() const { return glm::lookAt(pos, pos + front(), {0,1,0}); }
};

static Camera g_cam;
static bool g_firstMouse = true;
static float g_lastMx = 0, g_lastMy = 0;
static bool g_rmbLook = false;

struct Part { glm::vec3 pos, size, color; };

static std::vector<char> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg();
    f.seekg(0);
    std::vector<char> buf((size_t)sz);
    f.read(buf.data(), sz);
    return buf;
}

static std::string exeDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf);
    auto s = p.find_last_of("\\/");
    return s == std::string::npos ? "." : p.substr(0, s);
#else
    return ".";
#endif
}

static std::string findStudPath() {
    for (const auto& c : { exeDir() + "/assets/textures/stud.png", "assets/textures/stud.png" }) {
        if (FILE* f = std::fopen(c.c_str(), "rb")) { std::fclose(f); return c; }
    }
    return exeDir() + "/assets/textures/stud.png";
}

static std::string findSpv(const char* name) {
    for (const auto& c : { exeDir() + "/shaders/" + std::string(name),
                           std::string("shaders/") + name,
                           std::string("../shaders/") + name }) {
        if (FILE* f = std::fopen(c.c_str(), "rb")) { std::fclose(f); return c; }
    }
    return std::string("shaders/") + name;
}

struct VkCtx {
    GLFWwindow* win = nullptr;
    VkInstance instance{};
    VkDebugUtilsMessengerEXT dbg{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice phys{};
    VkDevice device{};
    VkQueue gfxQueue{};
    VkQueue presentQueue{};
    uint32_t gfxFamily = 0, presentFamily = 0;
    VkSwapchainKHR swapchain{};
    std::vector<VkImage> scImages;
    std::vector<VkImageView> scViews;
    VkFormat scFmt{};
    VkExtent2D scExt{};
    VkRenderPass renderPass{};
    VkDescriptorSetLayout dsl{};
    VkPipelineLayout pipeLayout{};
    VkPipeline pipeline{};
    std::vector<VkFramebuffer> fbs;
    VkCommandPool cmdPool{};
    std::vector<VkCommandBuffer> cmdBufs;
    std::vector<VkSemaphore> imgAvail, renderDone;
    std::vector<VkFence> inflight;
    size_t frame = 0;
    VkBuffer vbo{}, ubo{};
    VkDeviceMemory vboMem{}, uboMem{};
    VkDescriptorPool descPool{};
    VkDescriptorSet descSet{};
    VkImage studImg{};
    VkDeviceMemory studMem{};
    VkImageView studView{};
    VkSampler studSampler{};
    bool validation = false;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCb(VkDebugUtilsMessageSeverityFlagBitsEXT sev,
    VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
    if (sev >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::fprintf(stderr, "Vulkan: %s\n", data->pMessage);
    return VK_FALSE;
}

static bool checkLayer(const char* name) {
    uint32_t n = 0;
    vkEnumerateInstanceLayerProperties(&n, nullptr);
    std::vector<VkLayerProperties> layers(n);
    vkEnumerateInstanceLayerProperties(&n, layers.data());
    for (auto& l : layers) if (!strcmp(l.layerName, name)) return true;
    return false;
}

static uint32_t findMemType(VkPhysicalDevice phys, uint32_t filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem;
    vkGetPhysicalDeviceMemoryProperties(phys, &mem);
    for (uint32_t i = 0; i < mem.memoryTypeCount; ++i)
        if ((filter & (1u << i)) && (mem.memoryTypes[i].propertyFlags & props) == props) return i;
    return 0;
}

static VkShaderModule loadSpv(VkDevice dev, const std::string& path) {
    auto code = readFile(path);
    if (code.empty()) { std::fprintf(stderr, "SPIR-V mancante: %s\n", path.c_str()); return VK_NULL_HANDLE; }
    VkShaderModuleCreateInfo ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod{};
    vkCreateShaderModule(dev, &ci, nullptr, &mod);
    return mod;
}

struct FrameUBO {
    alignas(16) glm::mat4 view, proj;
    alignas(16) glm::vec3 sunDir;
    float studScale;
    alignas(16) glm::vec3 camPos;
    float _pad;
};

struct PushPC {
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec3 color;
    float _pad;
};

static void createCubeMesh(VkCtx& c, float* outVerts, size_t nFloats) {
    const float v[] = {
        -.5f,-.5f,.5f,0,0,1, .5f,-.5f,.5f,0,0,1, .5f,.5f,.5f,0,0,1,
        -.5f,-.5f,.5f,0,0,1, .5f,.5f,.5f,0,0,1, -.5f,.5f,.5f,0,0,1,
        -.5f,-.5f,-.5f,0,0,-1, -.5f,.5f,-.5f,0,0,-1, .5f,.5f,-.5f,0,0,-1,
        -.5f,-.5f,-.5f,0,0,-1, .5f,.5f,-.5f,0,0,-1, .5f,-.5f,-.5f,0,0,-1,
         .5f,-.5f,-.5f,1,0,0, .5f,.5f,-.5f,1,0,0, .5f,.5f,.5f,1,0,0,
         .5f,-.5f,-.5f,1,0,0, .5f,.5f,.5f,1,0,0, .5f,-.5f,.5f,1,0,0,
        -.5f,-.5f,-.5f,-1,0,0, -.5f,-.5f,.5f,-1,0,0, -.5f,.5f,.5f,-1,0,0,
        -.5f,-.5f,-.5f,-1,0,0, -.5f,.5f,.5f,-1,0,0, -.5f,.5f,-.5f,-1,0,0,
        -.5f,.5f,-.5f,0,1,0, -.5f,.5f,.5f,0,1,0, .5f,.5f,.5f,0,1,0,
        -.5f,.5f,-.5f,0,1,0, .5f,.5f,.5f,0,1,0, .5f,.5f,-.5f,0,1,0,
        -.5f,-.5f,-.5f,0,-1,0, .5f,-.5f,-.5f,0,-1,0, .5f,-.5f,.5f,0,-1,0,
        -.5f,-.5f,-.5f,0,-1,0, .5f,-.5f,.5f,0,-1,0, -.5f,-.5f,.5f,0,-1,0,
    };
    memcpy(outVerts, v, std::min(nFloats * sizeof(float), sizeof(v)));
}

static bool initVulkan(VkCtx& c, GLFWwindow* win) {
    c.win = win;
    uint32_t extCount = 0;
    const char** glfwExt = glfwGetRequiredInstanceExtensions(&extCount);
    std::vector<const char*> ext(glfwExt, glfwExt + extCount);
    std::vector<const char*> layers;
    c.validation = checkLayer("VK_LAYER_KHRONOS_validation");
    if (c.validation) layers.push_back("VK_LAYER_KHRONOS_validation");

    VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app.apiVersion = VK_API_VERSION_1_2;
    app.pApplicationName = "Blocky";

    VkInstanceCreateInfo ici{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = (uint32_t)ext.size();
    ici.ppEnabledExtensionNames = ext.data();
    ici.enabledLayerCount = (uint32_t)layers.size();
    ici.ppEnabledLayerNames = layers.data();
    if (vkCreateInstance(&ici, nullptr, &c.instance) != VK_SUCCESS) return false;

    if (c.validation) {
        auto fp = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(c.instance, "vkCreateDebugUtilsMessengerEXT");
        if (fp) {
            VkDebugUtilsMessengerCreateInfoEXT dci{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
            dci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            dci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
            dci.pfnUserCallback = debugCb;
            fp(c.instance, &dci, nullptr, &c.dbg);
        }
    }

    if (glfwCreateWindowSurface(c.instance, win, nullptr, &c.surface) != VK_SUCCESS) return false;

    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(c.instance, &devCount, nullptr);
    if (!devCount) return false;
    std::vector<VkPhysicalDevice> devs(devCount);
    vkEnumeratePhysicalDevices(c.instance, &devCount, devs.data());
    c.phys = devs[0];

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(c.phys, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(c.phys, &qCount, qProps.data());
    for (uint32_t i = 0; i < qCount; ++i) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(c.phys, i, c.surface, &present);
        if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) c.gfxFamily = i;
        if (present) c.presentFamily = i;
    }

    float prio = 1.f;
    std::set<uint32_t> unique = { c.gfxFamily, c.presentFamily };
    std::vector<VkDeviceQueueCreateInfo> qcis;
    for (uint32_t q : unique) {
        VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        qci.queueFamilyIndex = q;
        qci.queueCount = 1;
        qci.pQueuePriorities = &prio;
        qcis.push_back(qci);
    }

    const char* devExt[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount = (uint32_t)qcis.size();
    dci.pQueueCreateInfos = qcis.data();
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = devExt;
    dci.enabledLayerCount = (uint32_t)layers.size();
    dci.ppEnabledLayerNames = layers.data();
    if (vkCreateDevice(c.phys, &dci, nullptr, &c.device) != VK_SUCCESS) return false;

    vkGetDeviceQueue(c.device, c.gfxFamily, 0, &c.gfxQueue);
    vkGetDeviceQueue(c.device, c.presentFamily, 0, &c.presentQueue);
    return true;
}

static bool createSwapchain(VkCtx& c) {
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(c.phys, c.surface, &caps);
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(c.phys, c.surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(c.phys, c.surface, &fmtCount, fmts.data());
    c.scFmt = fmts[0].format;
    for (auto& f : fmts) if (f.format == VK_FORMAT_B8G8R8A8_SRGB) { c.scFmt = f.format; break; }

    c.scExt = caps.currentExtent;
    if (c.scExt.width == UINT32_MAX) {
        int w, h;
        glfwGetFramebufferSize(c.win, &w, &h);
        c.scExt = { (uint32_t)w, (uint32_t)h };
    }

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount && imgCount > caps.maxImageCount) imgCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    sci.surface = c.surface;
    sci.minImageCount = imgCount;
    sci.imageFormat = c.scFmt;
    sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = c.scExt;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    uint32_t qIdx[] = { c.gfxFamily, c.presentFamily };
    sci.imageSharingMode = c.gfxFamily != c.presentFamily ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    sci.queueFamilyIndexCount = c.gfxFamily != c.presentFamily ? 2u : 0u;
    sci.pQueueFamilyIndices = c.gfxFamily != c.presentFamily ? qIdx : nullptr;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(c.device, &sci, nullptr, &c.swapchain) != VK_SUCCESS) return false;

    uint32_t n = 0;
    vkGetSwapchainImagesKHR(c.device, c.swapchain, &n, nullptr);
    c.scImages.resize(n);
    vkGetSwapchainImagesKHR(c.device, c.swapchain, &n, c.scImages.data());
    c.scViews.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
        VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ivci.image = c.scImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = c.scFmt;
        ivci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCreateImageView(c.device, &ivci, nullptr, &c.scViews[i]);
    }
    return true;
}

static bool createRenderPass(VkCtx& c) {
    VkAttachmentDescription color{};
    color.format = c.scFmt;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;
    return vkCreateRenderPass(c.device, &rpci, nullptr, &c.renderPass) == VK_SUCCESS;
}

static bool createPipeline(VkCtx& c) {
    VkDescriptorSetLayoutBinding binds[2]{};
    binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binds[0].descriptorCount = 1; binds[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    binds[1].binding = 0; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binds[1].descriptorCount = 1; binds[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    dslci.bindingCount = 2;
    VkDescriptorSetLayoutBinding all[2] = { binds[0], {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} };
    dslci.pBindings = all;
    if (vkCreateDescriptorSetLayout(c.device, &dslci, nullptr, &c.dsl) != VK_SUCCESS) return false;

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(PushPC);

    VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &c.dsl;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(c.device, &plci, nullptr, &c.pipeLayout) != VK_SUCCESS) return false;

    auto vertPath = findSpv("part.vert.spv");
    auto fragPath = findSpv("part_stud.frag.spv");
    VkShaderModule vs = loadSpv(c.device, vertPath);
    VkShaderModule fs = loadSpv(c.device, fragPath);
    if (!vs || !fs) return false;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main" };
    stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main" };

    VkVertexInputBindingDescription bind{ 0, 6 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attrs[2] = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float) },
    };
    VkPipelineVertexInputStateCreateInfo vis{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vis.vertexBindingDescriptionCount = 1;
    vis.pVertexBindingDescriptions = &bind;
    vis.vertexAttributeDescriptionCount = 2;
    vis.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{ 0, 0, (float)c.scExt.width, (float)c.scExt.height, 0, 1 };
    VkRect2D sc{ {0,0}, c.scExt };
    VkPipelineViewportStateCreateInfo vps{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vps.viewportCount = 1; vps.pViewports = &vp;
    vps.scissorCount = 1; vps.pScissors = &sc;

    VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.f;

    VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState ba{};
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1; cb.pAttachments = &ba;

    VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dci{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dci.dynamicStateCount = 2; dci.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo gpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    gpci.stageCount = 2; gpci.pStages = stages;
    gpci.pVertexInputState = &vis;
    gpci.pInputAssemblyState = &ia;
    gpci.pViewportState = &vps;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState = &ms;
    gpci.pDepthStencilState = &ds;
    gpci.pColorBlendState = &cb;
    gpci.pDynamicState = &dci;
    gpci.layout = c.pipeLayout;
    gpci.renderPass = c.renderPass;
    gpci.subpass = 0;

    bool ok = vkCreateGraphicsPipelines(c.device, VK_NULL_HANDLE, 1, &gpci, nullptr, &c.pipeline) == VK_SUCCESS;
    vkDestroyShaderModule(c.device, vs, nullptr);
    vkDestroyShaderModule(c.device, fs, nullptr);
    return ok;
}

static bool createDepth(VkCtx& c, VkFormat& depthFmt, VkImage& depthImg, VkDeviceMemory& depthMem, VkImageView& depthView) {
    depthFmt = VK_FORMAT_D32_SFLOAT;
    VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = depthFmt;
    ici.extent = { c.scExt.width, c.scExt.height, 1 };
    ici.mipLevels = 1; ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (vkCreateImage(c.device, &ici, nullptr, &depthImg) != VK_SUCCESS) return false;
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(c.device, depthImg, &req);
    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = findMemType(c.phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(c.device, &mai, nullptr, &depthMem);
    vkBindImageMemory(c.device, depthImg, depthMem, 0);
    VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ivci.image = depthImg;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = depthFmt;
    ivci.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    return vkCreateImageView(c.device, &ivci, nullptr, &depthView) == VK_SUCCESS;
}

static bool uploadStudTexture(VkCtx& c, const std::string& path) {
    stbi_set_flip_vertically_on_load(true);
    int w, h, ch;
    stbi_uc* px = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!px) return false;

    VkBuffer staging{};
    VkDeviceMemory stagingMem{};
    VkDeviceSize sz = (VkDeviceSize)w * h * 4;
    VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size = sz;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    vkCreateBuffer(c.device, &bci, nullptr, &staging);
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(c.device, staging, &req);
    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = findMemType(c.phys, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(c.device, &mai, nullptr, &stagingMem);
    vkBindBufferMemory(c.device, staging, stagingMem, 0);
    void* mapped = nullptr;
    vkMapMemory(c.device, stagingMem, 0, sz, 0, &mapped);
    memcpy(mapped, px, (size_t)sz);
    vkUnmapMemory(c.device, stagingMem);
    stbi_image_free(px);

    VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_SRGB;
    ici.extent = { (uint32_t)w, (uint32_t)h, 1 };
    ici.mipLevels = 1; ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    vkCreateImage(c.device, &ici, nullptr, &c.studImg);
    vkGetImageMemoryRequirements(c.device, c.studImg, &req);
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = findMemType(c.phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(c.device, &mai, nullptr, &c.studMem);
    vkBindImageMemory(c.device, c.studImg, c.studMem, 0);

    VkCommandPoolCreateInfo cpci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cpci.queueFamilyIndex = c.gfxFamily;
    VkCommandPool tmpPool{};
    vkCreateCommandPool(c.device, &cpci, nullptr, &tmpPool);
    VkCommandBuffer cmd{};
    VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool = tmpPool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
    vkAllocateCommandBuffers(c.device, &cbai, &cmd);

    VkCommandBufferBeginInfo cbi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &cbi);

    VkImageMemoryBarrier bar{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    bar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.image = c.studImg;
    bar.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    bar.srcAccessMask = 0;
    bar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, nullptr, 0, nullptr, 1, &bar);

    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };
    vkCmdCopyBufferToImage(cmd, staging, c.studImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    bar.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr, 0, nullptr, 1, &bar);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    vkQueueSubmit(c.gfxQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(c.gfxQueue);
    vkFreeCommandBuffers(c.device, tmpPool, 1, &cmd);
    vkDestroyCommandPool(c.device, tmpPool, nullptr);
    vkDestroyBuffer(c.device, staging, nullptr);
    vkFreeMemory(c.device, stagingMem, nullptr);

    VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ivci.image = c.studImg;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = VK_FORMAT_R8G8B8A8_SRGB;
    ivci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCreateImageView(c.device, &ivci, nullptr, &c.studView);

    VkSamplerCreateInfo sci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(c.device, &sci, nullptr, &c.studSampler);
    return true;
}

static void drawPart(VkCtx& c, VkCommandBuffer cmd, const glm::mat4& model, const glm::vec3& color) {
    PushPC pc{};
    pc.model = model;
    pc.color = color;
    vkCmdPushConstants(cmd, c.pipeLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushPC), &pc);
    vkCmdDraw(cmd, 36, 1, 0, 0);
}

static void onResize(GLFWwindow*, int w, int h) { g_w = std::max(w, 1); g_h = std::max(h, 1); }

static void onMouse(GLFWwindow*, double x, double y) {
    if (!g_rmbLook) return;
    if (g_firstMouse) { g_lastMx = (float)x; g_lastMy = (float)y; g_firstMouse = false; }
    float dx = (float)x - g_lastMx, dy = g_lastMy - (float)y;
    g_lastMx = (float)x; g_lastMy = (float)y;
    g_cam.yaw += dx * 0.12f;
    g_cam.pitch = glm::clamp(g_cam.pitch + dy * 0.12f, -89.f, 89.f);
}

static void onMouseBtn(GLFWwindow*, int btn, int action, int) {
    if (btn == GLFW_MOUSE_BUTTON_RIGHT) {
        g_rmbLook = (action == GLFW_PRESS);
        g_firstMouse = true;
        glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR,
            g_rmbLook ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* win = glfwCreateWindow(g_w, g_h, "Roblox Engine — Vulkan (PC)", nullptr, nullptr);
    if (!win) return -1;
    setWindowIconR(win);
    glfwSetFramebufferSizeCallback(win, onResize);
    glfwSetCursorPosCallback(win, onMouse);
    glfwSetMouseButtonCallback(win, onMouseBtn);

    VkCtx ctx{};
    if (!initVulkan(ctx, win)) { std::fprintf(stderr, "Vulkan init fallita\n"); return -1; }
    if (!createSwapchain(ctx)) return -1;
    if (!createRenderPass(ctx)) return -1;

    VkFormat depthFmt{};
    VkImage depthImg{}; VkDeviceMemory depthMem{}; VkImageView depthView{};
    createDepth(ctx, depthFmt, depthImg, depthMem, depthView);

    vkDestroyRenderPass(ctx.device, ctx.renderPass, nullptr);
    VkAttachmentDescription atts[2]{};
    atts[0].format = ctx.scFmt; atts[0].samples = VK_SAMPLE_COUNT_1_BIT;
    atts[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; atts[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    atts[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; atts[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    atts[1].format = depthFmt; atts[1].samples = VK_SAMPLE_COUNT_1_BIT;
    atts[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; atts[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    atts[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; atts[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    atts[1].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub{}; sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1; sub.pColorAttachments = &colorRef;
    sub.pDepthStencilAttachment = &depthRef;
    VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpci.attachmentCount = 2; rpci.pAttachments = atts;
    rpci.subpassCount = 1; rpci.pSubpasses = &sub;
    vkCreateRenderPass(ctx.device, &rpci, nullptr, &ctx.renderPass);

    if (!createPipeline(ctx)) { std::fprintf(stderr, "Pipeline fallita — compila gli shader SPIR-V\n"); return -1; }

    ctx.fbs.resize(ctx.scViews.size());
    for (size_t i = 0; i < ctx.scViews.size(); ++i) {
        VkImageView att[] = { ctx.scViews[i], depthView };
        VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbci.renderPass = ctx.renderPass;
        fbci.attachmentCount = 2; fbci.pAttachments = att;
        fbci.width = ctx.scExt.width; fbci.height = ctx.scExt.height; fbci.layers = 1;
        vkCreateFramebuffer(ctx.device, &fbci, nullptr, &ctx.fbs[i]);
    }

    float cubeData[36 * 6];
    createCubeMesh(ctx, cubeData, 36 * 6);
    VkDeviceSize vboSize = sizeof(cubeData);
    VkBufferCreateInfo vbci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vbci.size = vboSize;
    vbci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vkCreateBuffer(ctx.device, &vbci, nullptr, &ctx.vbo);
    VkMemoryRequirements vreq{};
    vkGetBufferMemoryRequirements(ctx.device, ctx.vbo, &vreq);
    VkMemoryAllocateInfo vmai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    vmai.allocationSize = vreq.size;
    vmai.memoryTypeIndex = findMemType(ctx.phys, vreq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(ctx.device, &vmai, nullptr, &ctx.vboMem);
    vkBindBufferMemory(ctx.device, ctx.vbo, ctx.vboMem, 0);
    void* vmap = nullptr;
    vkMapMemory(ctx.device, ctx.vboMem, 0, vboSize, 0, &vmap);
    memcpy(vmap, cubeData, (size_t)vboSize);
    vkUnmapMemory(ctx.device, ctx.vboMem);

    VkDeviceSize uboSize = sizeof(FrameUBO);
    VkBufferCreateInfo ubci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    ubci.size = uboSize;
    ubci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vkCreateBuffer(ctx.device, &ubci, nullptr, &ctx.ubo);
    vkGetBufferMemoryRequirements(ctx.device, ctx.ubo, &ureq);
    vmai.allocationSize = ureq.size;
    vmai.memoryTypeIndex = findMemType(ctx.phys, ureq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(ctx.device, &vmai, nullptr, &ctx.uboMem);
    vkBindBufferMemory(ctx.device, ctx.ubo, ctx.uboMem, 0);

    uploadStudTexture(ctx, findStudPath());

    VkDescriptorPoolSize ps[2] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    };
    VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpci.maxSets = 1; dpci.poolSizeCount = 2; dpci.pPoolSizes = ps;
    vkCreateDescriptorPool(ctx.device, &dpci, nullptr, &ctx.descPool);
    VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    dsai.descriptorPool = ctx.descPool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &ctx.dsl;
    vkAllocateDescriptorSets(ctx.device, &dsai, &ctx.descSet);

    VkDescriptorBufferInfo dbi{ ctx.ubo, 0, uboSize };
    VkDescriptorImageInfo dii{ ctx.studSampler, ctx.studView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet writes[2]{};
    writes[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, ctx.descSet, 0, 0, 1,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &dbi, nullptr };
    writes[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, ctx.descSet, 1, 0, 1,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &dii, nullptr, nullptr };
    vkUpdateDescriptorSets(ctx.device, 2, writes, 0, nullptr);

    VkCommandPoolCreateInfo cpci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cpci.queueFamilyIndex = ctx.gfxFamily;
    vkCreateCommandPool(ctx.device, &cpci, nullptr, &ctx.cmdPool);
    ctx.cmdBufs.resize(ctx.fbs.size());
    VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool = ctx.cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = (uint32_t)ctx.cmdBufs.size();
    vkAllocateCommandBuffers(ctx.device, &cbai, ctx.cmdBufs.data());

    const size_t maxFrames = ctx.fbs.size();
    ctx.imgAvail.resize(maxFrames); ctx.renderDone.resize(maxFrames); ctx.inflight.resize(maxFrames);
    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < maxFrames; ++i) {
        vkCreateSemaphore(ctx.device, &sci, nullptr, &ctx.imgAvail[i]);
        vkCreateSemaphore(ctx.device, &sci, nullptr, &ctx.renderDone[i]);
        vkCreateFence(ctx.device, &fci, nullptr, &ctx.inflight[i]);
    }

    Part base{ {0,-1,0}, {120,2,120}, {0.55f,0.56f,0.55f} };
    std::vector<Part> parts = { {{8,2,-4}, {4,4,4}, {0.77f,0.16f,0.11f}} };
    glm::vec3 sun = glm::normalize(glm::vec3(-0.4f, -1.f, -0.3f));

    float lastT = (float)glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        float now = (float)glfwGetTime();
        float dt = std::min(now - lastT, 0.05f);
        lastT = now;

        float spd = 14.f * dt * (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) ? 3.f : 1.f);
        glm::vec3 f = g_cam.front(), r = glm::normalize(glm::cross(f, glm::vec3(0,1,0)));
        if (glfwGetKey(win, GLFW_KEY_W)) g_cam.pos += f * spd;
        if (glfwGetKey(win, GLFW_KEY_S)) g_cam.pos -= f * spd;
        if (glfwGetKey(win, GLFW_KEY_A)) g_cam.pos -= r * spd;
        if (glfwGetKey(win, GLFW_KEY_D)) g_cam.pos += r * spd;
        if (glfwGetKey(win, GLFW_KEY_SPACE)) g_cam.pos.y += spd;
        if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL)) g_cam.pos.y -= spd;
        if (glfwGetKey(win, GLFW_KEY_ESCAPE)) break;

        vkWaitForFences(ctx.device, 1, &ctx.inflight[ctx.frame], VK_TRUE, UINT64_MAX);
        uint32_t imgIdx = 0;
        VkResult acq = vkAcquireNextImageKHR(ctx.device, ctx.swapchain, UINT64_MAX,
            ctx.imgAvail[ctx.frame], VK_NULL_HANDLE, &imgIdx);
        if (acq == VK_ERROR_OUT_OF_DATE_KHR) continue;
        vkResetFences(ctx.device, 1, &ctx.inflight[ctx.frame]);

        float aspect = g_w / (float)g_h;
        glm::mat4 proj = glm::perspective(glm::radians(g_cam.fov), aspect, 0.1f, 800.f);
        proj[1][1] *= -1.f;
        glm::mat4 view = g_cam.view();

        FrameUBO ubo{};
        ubo.view = view; ubo.proj = proj;
        ubo.sunDir = sun; ubo.studScale = kStudTile;
        ubo.camPos = g_cam.pos;
        void* umap = nullptr;
        vkMapMemory(ctx.device, ctx.uboMem, 0, sizeof(FrameUBO), 0, &umap);
        memcpy(umap, &ubo, sizeof(FrameUBO));
        vkUnmapMemory(ctx.device, ctx.uboMem);

        VkCommandBuffer cmd = ctx.cmdBufs[imgIdx];
        vkResetCommandBuffer(cmd, 0);
        VkCommandBufferBeginInfo cbi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        vkBeginCommandBuffer(cmd, &cbi);

        VkClearValue clears[2]{};
        clears[0].color = { {0.53f, 0.72f, 0.92f, 1.f} };
        clears[1].depthStencil = { 1.f, 0 };
        VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpbi.renderPass = ctx.renderPass;
        rpbi.framebuffer = ctx.fbs[imgIdx];
        rpbi.renderArea.extent = ctx.scExt;
        rpbi.clearValueCount = 2; rpbi.pClearValues = clears;
        vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{ 0, 0, (float)ctx.scExt.width, (float)ctx.scExt.height, 0, 1 };
        VkRect2D sc{ {0,0}, ctx.scExt };
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline);
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &ctx.vbo, &off);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeLayout, 0, 1, &ctx.descSet, 0, nullptr);

        glm::mat4 bm = glm::scale(glm::translate(glm::mat4(1.f), base.pos), base.size);
        drawPart(ctx, cmd, bm, base.color);
        for (auto& p : parts) {
            glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), p.pos), p.size);
            drawPart(ctx, cmd, m, p.color);
        }

        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);

        VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.waitSemaphoreCount = 1; si.pWaitSemaphores = &ctx.imgAvail[ctx.frame];
        si.pWaitDstStageMask = &wait;
        si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
        si.signalSemaphoreCount = 1; si.pSignalSemaphores = &ctx.renderDone[ctx.frame];
        vkQueueSubmit(ctx.gfxQueue, 1, &si, ctx.inflight[ctx.frame]);

        VkPresentInfoKHR pi{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &ctx.renderDone[ctx.frame];
        pi.swapchainCount = 1; pi.pSwapchains = &ctx.swapchain; pi.pImageIndices = &imgIdx;
        vkQueuePresentKHR(ctx.presentQueue, &pi);

        ctx.frame = (ctx.frame + 1) % maxFrames;
    }

    vkDeviceWaitIdle(ctx.device);
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
