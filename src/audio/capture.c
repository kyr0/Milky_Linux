#include "capture.h"

// double buffers
FrameBuffer frameBuffers[BUFFER_COUNT];
int currentWriteBuffer = 0;
int currentRenderBuffer = 0;

// mutex and condition variable
pthread_mutex_t bufferMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bufferCond = PTHREAD_COND_INITIALIZER;

// rendering thread handle
pthread_t renderThread;

// global variable to store the reference time
static struct timeval app_start_time;

// static variables to track time and rendering
static struct timespec lastRenderTime = {0, 0};

const char* vertex_shader_src = 
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"      // input vertex position
    "layout(location = 1) in vec2 aTexCoord;\n" // input texture coordinates
    "out vec2 TexCoord;\n"                      // output texture coordinates
    "uniform float curvatureStrength;      // Strength of the curvature\n"
    "\n"
    "void main() {\n"
    "    // Compute distance from the center of the screen\n"
    "    vec2 centeredPos = aPos * 2.0 - 1.0; // Convert to [-1, 1] range\n"
    "    float distanceSq = dot(centeredPos, centeredPos);\n"
    "\n"
    "    // Apply curvature effect\n"
    "    float curvature = curvatureStrength * distanceSq;\n"
    "    vec3 curvedPosition = vec3(centeredPos, -curvature);\n"
    "\n"
    "    // Map back to screen space\n"
    "    gl_Position = vec4(curvedPosition.xy * 0.5 + 0.5, curvedPosition.z, 1.0);\n"
    "\n"
    "    // Pass through the texture coordinates\n"
    "    TexCoord = aTexCoord;\n"
    "}\n";
const char* fragment_shader_src = 
"#version 330 core\n"
"\n"
"// Input texture coordinates from the vertex shader\n"
"in vec2 TexCoord;\n"
"\n"
"// Output fragment color\n"
"out vec4 FragColor;\n"
"\n"
"// Existing uniforms\n"
"uniform sampler2D texture1;\n"
"uniform float vignetteIntensity; // Intensity of the vignette effect\n"
"uniform float zoomFactor;        // Zoom effect multiplier\n"
"uniform vec2 center;             // Dynamic center point\n"
"uniform float time;              // Time for animation\n"
"uniform float rotationSpeed;     // Speed of rotation\n"
"uniform float blurStrength;      // Strength of radial blur\n"
"\n"
"// New uniforms from the integrated shader\n"
"uniform int Angle_Steps;         // Number of angular steps (Default: 3, Range: 1 - 20)\n"
"uniform int Radius_Steps;        // Number of radial steps (Default: 9, Range: 0 - 20)\n"
"uniform float ampFactor;         // Amplification factor (Default: 2.0)\n"
"uniform vec2 uv_pixel_interval;  // Size of a single pixel in UV coordinates\n"
"\n"
"// Constants for Vignette Effect\n"
"const float MAX_ROTATION_DEGREES = 15.0f;\n"
"const float MIN_ROTATION_DEGREES = 1.0f;\n"
"const float DEG_TO_RAD = 3.1415926535897932384626433832795 / 180.0f;\n"
"\n"
"// Vignette Parameters with Default Values\n"
"const float innerRadius = 0.9;\n"
"const float outerRadius = 1.5;\n"
"const float opacity = 0.8;\n"
"\n"
"// Radial blur helper function\n"
"vec4 radialBlur(sampler2D tex, vec2 uv, vec2 center, float blurStrength) {\n"
"    vec4 result = vec4(0.0);\n"
"    float totalWeight = 0.0;\n"
"    for (float t = 0.0; t < 1.0; t += 0.1) {\n"
"        vec2 sampleUV = mix(uv, center, t * blurStrength);\n"
"        vec4 sample = texture(tex, sampleUV);\n"
"        float weight = 1.0 - t;\n"
"        result += sample * weight;\n"
"        totalWeight += weight;\n"
"    }\n"
"    return result / totalWeight;\n"
"}\n"
"\n"
"void main() {\n"
"    // === Existing Shader Logic === //\n"
"\n"
"    // Rotate the texture coordinates around the center\n"
"    float angle = time * rotationSpeed;\n"
"    mat2 rotationMatrix = mat2(cos(angle), -sin(angle),\n"
"                               sin(angle),  cos(angle));\n"
"    vec2 rotatedUV = rotationMatrix * (TexCoord - center) + center;\n"
"\n"
"    // Apply zoom factor\n"
"    vec2 zoomUV = center + (rotatedUV - center) * zoomFactor;\n"
"\n"
"    // Apply radial blur\n"
"    vec4 blurred = radialBlur(texture1, zoomUV, center, blurStrength);\n"
"\n"
"    // Apply center dim effect\n"
"    vec2 offset = TexCoord - center;\n"
"    float distanceToCenter = length(offset);\n"
"    float smoothValue = smoothstep(0.0, 0.9, distanceToCenter);\n"
"    float dimFactor = mix(1.0 - 0.3, 1.0, smoothValue);\n"
"    dimFactor = clamp(dimFactor, 0.0, 1.0);\n"
"    blurred.rgb *= clamp(dimFactor, 0.0, 1.0);\n"
"\n"
"    // Apply gamma correction\n"
"    float gamma = 3; // Standard gamma correction\n"
"    blurred.rgb = pow(blurred.rgb, vec3(1.0 / gamma));\n"
"\n"
"    // Clamp the final color values to valid range\n"
"    blurred.rgb = clamp(blurred.rgb, 0.0, 1.0);\n"
"\n"
"    // === Integrated Radial Steps Logic === //\n"
"\n"
"    // Clamp the uniform values to their respective ranges\n"
"    int radiusSteps = clamp(Radius_Steps, 0, 20);\n"
"    int angleSteps = clamp(Angle_Steps, 1, 20);\n"
"\n"
"    // Define minimum and maximum radius based on pixel interval\n"
"    float minRadius = 0.0 * uv_pixel_interval.y;  // Adjust if needed\n"
"    float maxRadius = 10.0 * uv_pixel_interval.y; // Adjust if needed\n"
"\n"
"    // Sample the original color at the current fragment\n"
"    vec4 c0 = texture(texture1, TexCoord);\n"
"    vec4 outputPixel = c0;\n"
"    vec4 accumulatedColor = vec4(0.0);\n"
"\n"
"    // Calculate total number of steps and deltas\n"
"    int totalSteps = radiusSteps * angleSteps;\n"
"    float angleDelta = (2.0 * 3.1415926535897932384626433832795) / float(angleSteps);\n"
"    float radiusDelta = (maxRadius - minRadius) / float(radiusSteps);\n"
"\n"
"    // Iterate over each radius step\n"
"    for (int radiusStep = 0; radiusStep < radiusSteps; radiusStep++) {\n"
"        float radius = minRadius + float(radiusStep) * radiusDelta;\n"
"\n"
"        // Iterate over each angle step\n"
"        for (int angleStep = 0; angleStep < angleSteps; angleStep++) {\n"
"            float currentAngle = float(angleStep) * angleDelta;\n"
"\n"
"            // Calculate the offset coordinates\n"
"            float xDiff = radius * cos(currentAngle);\n"
"            float yDiff = radius * sin(currentAngle);\n"
"            vec2 currentCoord = TexCoord + vec2(xDiff, yDiff);\n"
"\n"
"            // Sample the texture at the offset coordinates\n"
"            vec4 currentColor = texture(texture1, currentCoord);\n"
"\n"
"            // Calculate the weight for the current sample\n"
"            float currentFraction = float(radiusSteps + 1 - radiusStep) / float(radiusSteps + 1);\n"
"\n"
"            // Accumulate the weighted color\n"
"            accumulatedColor += currentFraction * currentColor / float(totalSteps);\n"
"        }\n"
"    }\n"
"\n"
"    // Add the accumulated color to the output pixel, scaled by ampFactor\n"
"    outputPixel += accumulatedColor * ampFactor;\n"
"\n"
"    // === Combine Both Effects === //\n"
"\n"
"    // Blend the blurred color and the radial steps effect\n"
"    float blendFactor = 0.5;\n"
"    vec4 finalColor = mix(blurred, outputPixel, blendFactor);\n"
"\n"
"    // === Vignette Effect === //\n"
"\n"
"    // Calculate the normalized coordinates relative to the center\n"
"    vec2 normCoord = (TexCoord - center) * 2.0;\n"
"    float dist = length(normCoord);\n"
"\n"
"    // Calculate the vignette factor based on distance\n"
"    float vignette = smoothstep(innerRadius, outerRadius, dist);\n"
"\n"
"    // Apply the vignette effect by darkening the corners\n"
"    finalColor.rgb *= mix(1.0, 1.0 - opacity, vignette);\n"
"\n"
"    // Reduce saturation by 0.1\n"
"    float gray = dot(finalColor.rgb, vec3(0.299, 0.587, 0.114));\n"
"    vec3 desaturated = mix(vec3(gray), finalColor.rgb, 0.99);\n"
"\n"
"    // Increase contrast by 0.1\n"
"    float contrastFactor = 1.25;\n"
"    vec3 contrasted = (desaturated - 0.5) * contrastFactor + 0.5;\n"
"    contrasted = clamp(contrasted, 0.0, 1.0);\n"
"\n"
"    // === Increase Brightness === //\n"
"\n"
"    // Increase brightness by 0.1 \n"
"    float brightnessFactor = 1.4;\n"
"    vec3 brightened = contrasted * brightnessFactor;\n"
"    brightened = clamp(brightened, 0.0, 1.0);\n"
"\n"
"    // Output the final color with adjustments\n"
"    FragColor = vec4(brightened, finalColor.a);\n"
"}\n";


void initialize_timer() {
    gettimeofday(&app_start_time, NULL);
}

float performance_now() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    // calculate elapsed time in microseconds
    uint64_t elapsed_usec =
        (current_time.tv_sec - app_start_time.tv_sec) * 1000000ULL +
        (current_time.tv_usec - app_start_time.tv_usec);

    // convert elapsed time to milliseconds as float and return
    return (float)(elapsed_usec / 1000.0);
}

// shader compilation and linking function
GLuint compile_and_link_shaders(const char* vertex_src, const char* fragment_src) {
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_src, NULL);
    glCompileShader(vertex_shader);

    GLint success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
        fprintf(stderr, "ERROR::VERTEX_SHADER_COMPILATION_FAILED\n%s\n", info_log);
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_src, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
        fprintf(stderr, "ERROR::FRAGMENT_SHADER_COMPILATION_FAILED\n%s\n", info_log);
    }

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(shader_program, 512, NULL, info_log);
        fprintf(stderr, "ERROR::SHADER_PROGRAM_LINKING_FAILED\n%s\n", info_log);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return shader_program;
}

// setup vertex data
void setup_vertex_data(GLuint *VAO, GLuint *VBO, GLuint *EBO) {
    float vertices[] = {
        // positions      // texture coords
        -1.0f, -1.0f,     0.0f, 0.0f,
         1.0f, -1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f,     0.0f, 1.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, VAO);
    glGenBuffers(1, VBO);
    glGenBuffers(1, EBO);

    glBindVertexArray(*VAO);

    glBindBuffer(GL_ARRAY_BUFFER, *VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    glBindVertexArray(0); 
}

// for when the window is resized, we want to resize the viewport
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}  

int pulse_run(PulseAudio *pa) {
    int ret = 1;
    if (pa_mainloop_run(pa->mainloop, &ret) < 0) {
        fprintf(stderr, "pa_mainloop_run() failed.\n");
        return ret;
    }
    return ret;
} 

void pulse_quit(PulseAudio *pa, int ret) {
    printf(stdout, "Signaling PulseAudio to quit.\n");
    pa->mainloop_api->quit(pa->mainloop_api, ret);
}

void pulse_destroy(PulseAudio *pa) {
    if (pa->context) {
        pa_context_unref(pa->context);
        pa->context = NULL;
    }

    if (pa->signal) {
        pa_signal_free(pa->signal);
        pa_signal_done();
        pa->signal = NULL;
    }

    if (pa->mainloop) {
        pa_mainloop_free(pa->mainloop);
        pa->mainloop = NULL;
        pa->mainloop_api = NULL;
    }
} 


void exit_signal_callback(pa_mainloop_api *m, pa_signal_event *e, int sig, void *userdata) {
    PulseAudio *pa = (PulseAudio *)userdata;
    printf("Exit signal (free resources here for the moment!)");
    if (pa) pulse_quit(pa, 0);
}

static void window_close_callback(GLFWwindow* window) {
     exit(EXIT_SUCCESS);
}
void* render_thread_func(void *arg) {
    // Set OpenMP parameters
    omp_set_dynamic(10);
    omp_set_num_threads(12); 
    omp_set_nested(12);

    PulseAudio *pa = (PulseAudio *)arg; // receive PulseAudio pointer

    // Initialize GLFW and OpenGL in this thread
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW in render thread.\n");
        pthread_exit(NULL);
    }

    GLFWwindow *renderWindow = glfwCreateWindow(WIDTH, HEIGHT, "MilkyLinuxOSD", NULL, NULL);
    if (!renderWindow) {
        fprintf(stderr, "Failed to create GLFW window in render thread.\n");
        glfwTerminate();
        pthread_exit(NULL);
    }

    glfwSetFramebufferSizeCallback(renderWindow, framebuffer_size_callback);
    glfwMakeContextCurrent(renderWindow);
    glfwSetWindowCloseCallback(renderWindow, window_close_callback);

    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW in render thread.\n");
        glfwDestroyWindow(renderWindow);
        glfwTerminate();
        pthread_exit(NULL);
    }

    // Compile and link shaders
    GLuint localShaderProgram = compile_and_link_shaders(vertex_shader_src, fragment_shader_src);
    if (localShaderProgram == 0) {
        fprintf(stderr, "Failed to compile and link shaders.\n");
        glfwDestroyWindow(renderWindow);
        glfwTerminate();
        pthread_exit(NULL);
    }

    // Setup vertex data
    GLuint localVAO, localVBO, localEBO;
    setup_vertex_data(&localVAO, &localVBO, &localEBO);

    // Generate and bind local texture
    GLuint renderTexture;
    glGenTextures(1, &renderTexture);
    glBindTexture(GL_TEXTURE_2D, renderTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glUseProgram(localShaderProgram);

    // Set 'texture1' uniform to texture unit 0
    GLint texture1Loc = glGetUniformLocation(localShaderProgram, "texture1");
    if (texture1Loc != -1) {
        glUniform1i(texture1Loc, 0); // texture unit 0
    } else {
        fprintf(stderr, "Warning: 'texture1' uniform not found in shader program.\n");
    }

    // Activate texture unit 0 and bind the texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderTexture);

    // Retrieve uniform locations from the local shader program
    GLint localTimeLoc = glGetUniformLocation(localShaderProgram, "time");
    GLint localVignetteIntensityLoc = glGetUniformLocation(localShaderProgram, "vignetteIntensity");
    GLint localZoomFactorLoc = glGetUniformLocation(localShaderProgram, "zoomFactor");
    GLint localCenterLoc = glGetUniformLocation(localShaderProgram, "center");
    GLint localRotationSpeedLoc = glGetUniformLocation(localShaderProgram, "rotationSpeed");
    GLint localBlurStrengthLoc = glGetUniformLocation(localShaderProgram, "blurStrength");
    
    // New uniform locations
    GLint localAngleStepsLoc = glGetUniformLocation(localShaderProgram, "Angle_Steps");
    GLint localRadiusStepsLoc = glGetUniformLocation(localShaderProgram, "Radius_Steps");
    GLint localAmpFactorLoc = glGetUniformLocation(localShaderProgram, "ampFactor");
    GLint localUvPixelIntervalLoc = glGetUniformLocation(localShaderProgram, "uv_pixel_interval");

    // Check for uniform locations
    if (localTimeLoc == -1 || localVignetteIntensityLoc == -1 || localZoomFactorLoc == -1 ||
        localCenterLoc == -1 || localRotationSpeedLoc == -1 || localBlurStrengthLoc == -1 ||
        localAngleStepsLoc == -1 || localRadiusStepsLoc == -1 || localAmpFactorLoc == -1 ||
        localUvPixelIntervalLoc == -1) {
        fprintf(stderr, "Warning: One or more uniform locations not found in the shader program.\n");
    }

    // Set default values for the new uniforms
    if (localAngleStepsLoc != -1) {
        glUniform1i(localAngleStepsLoc, 5); // Default: 3
    }
    if (localRadiusStepsLoc != -1) {
        glUniform1i(localRadiusStepsLoc, 9); // Default: 9
    }
    if (localAmpFactorLoc != -1) {
        glUniform1f(localAmpFactorLoc, 2.0f); // Default: 2.0
    }

    // Set the uv_pixel_interval based on texture size
    if (localUvPixelIntervalLoc != -1) {
        glUniform2f(localUvPixelIntervalLoc, 1.0f / (float)WIDTH, 1.0f / (float)HEIGHT);
    }

    // Set other existing uniforms with default or desired values
    // Example values; adjust as necessary
    if (localTimeLoc != -1) glUniform1f(localTimeLoc, 0.0f);
    if (localVignetteIntensityLoc != -1) glUniform1f(localVignetteIntensityLoc, 1.1f);
    if (localZoomFactorLoc != -1) glUniform1f(localZoomFactorLoc, 0.995f);
    if (localCenterLoc != -1) glUniform2f(localCenterLoc, 0.5f, 0.5f);
    if (localRotationSpeedLoc != -1) glUniform1f(localRotationSpeedLoc, 0.0f);
    if (localBlurStrengthLoc != -1) glUniform1f(localBlurStrengthLoc, 0.001f);

    // Main rendering loop
    while (!glfwWindowShouldClose(renderWindow)) {
        pthread_mutex_lock(&bufferMutex);

        // Wait until the render buffer is ready or termination is signaled
        while (frameBuffers[currentRenderBuffer].ready == 0) {
            pthread_cond_wait(&bufferCond, &bufferMutex);
        }

        // Get the data to render
        uint8_t *renderFrame = frameBuffers[currentRenderBuffer].data;

        // Mark the buffer as not ready
        frameBuffers[currentRenderBuffer].ready = 0;

        pthread_mutex_unlock(&bufferMutex);

        // Update uniforms
        float currentTime = glfwGetTime();
        if (localTimeLoc != -1) glUniform1f(localTimeLoc, currentTime);
        // If curvatureStrength is no longer used, remove or update accordingly
        // Assuming curvatureStrength was a mistake and removing it:
        // if (localCurvatureStrengthLoc != -1) glUniform1f(localCurvatureStrengthLoc, 0.05f);
        if (localVignetteIntensityLoc != -1) glUniform1f(localVignetteIntensityLoc, 1.1f);
        if (localZoomFactorLoc != -1) glUniform1f(localZoomFactorLoc, 0.995f);
        if (localCenterLoc != -1) glUniform2f(localCenterLoc, 0.5f, 0.5f);
        if (localRotationSpeedLoc != -1) glUniform1f(localRotationSpeedLoc, 0.0f);
        if (localBlurStrengthLoc != -1) glUniform1f(localBlurStrengthLoc, 0.001f);

        // Optionally, update Angle_Steps, Radius_Steps, and ampFactor here if they are dynamic
        // For now, they are set once before the loop

        // Upload texture data
        glBindTexture(GL_TEXTURE_2D, renderTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, renderFrame);

        // Clear the screen
        glClear(GL_COLOR_BUFFER_BIT);

        // Render the quad
        glBindVertexArray(localVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        
        // Swap buffers and poll events
        glfwSwapBuffers(renderWindow);
        glfwPollEvents();
    }

    // After exiting the loop, signal PulseAudio to quit
    if (pa && pa->mainloop_api) {
        // Implement pulse_quit as per your PulseAudio setup
        // pulse_quit(pa, 0);
    }

    // Cleanup OpenGL resources
    glDeleteTextures(1, &renderTexture);
    glDeleteVertexArrays(1, &localVAO);
    glDeleteBuffers(1, &localVBO);
    glDeleteBuffers(1, &localEBO);
    glDeleteProgram(localShaderProgram);

    glfwDestroyWindow(renderWindow);
    glfwTerminate();

    pthread_exit(NULL);
}

void* pulse_thread_func(void *arg) {

    omp_set_dynamic(0);
    omp_set_num_threads(6); 
    omp_set_nested(6);

    PulseAudio *pa = (PulseAudio *)arg;
    int ret = pulse_run(pa);
    return (void *)(intptr_t)ret;
}

int pulse_initialize(PulseAudio *pa) {
    // create a new PulseAudio mainloop
    pa->mainloop = pa_mainloop_new();

    if (!pa->mainloop) {
        fprintf(stderr, "pa_mainloop_new() failed!\n");
        return 0;
    }

    // allocate a mainloop API
    pa->mainloop_api = pa_mainloop_get_api(pa->mainloop);

    if (pa_signal_init(pa->mainloop_api) != 0) {
        fprintf(stderr, "pa_signal_init() failed\n");
        return 0;
    }

    // allocate a signal for exiting (SIGINT)
    pa->signal = pa_signal_new(SIGINT, exit_signal_callback, pa);
    if (!pa->signal) {
        fprintf(stderr, "pa_signal_new() failed\n");
        return 0;
    }

    // ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    // allocate a new context for the mainloop API with name
    pa->context = pa_context_new(pa->mainloop_api, "Milky_PulseAudio_Listener");
    if (!pa->context) {
        fprintf(stderr, "pa_context_new() failed\n");
        return 0;
    }

    // connect to PulseAudio
    if (pa_context_connect(pa->context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
        fprintf(stderr, "pa_context_connect() failed: %s\n", pa_strerror(pa_context_errno(pa->context)));
        return 0;
    }
    pa_context_set_state_callback(pa->context, context_state_callback, pa);

    return 1;
}

// handles PulseAudio context state changes
void context_state_callback(pa_context *c, void *userdata) {
    assert(c && userdata);

    PulseAudio *pa = (PulseAudio *)userdata;

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
            fprintf(stderr, "PulseAudio connection established.\n");
            pa_context_get_server_info(c, server_info_callback, userdata);

            // subscribe to sink events for volume change notifications
            pa_context_set_subscribe_callback(c, subscribe_callback, userdata);
            pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SINK, NULL, NULL);

            // try to record system audio
            pa_sample_spec ss = {
                .format = PA_SAMPLE_U8, // 8-bit unsigned
                .rate = 44100,          // 44.1kHz sample rate
                .channels = 2           // Stereo
            };

            pa->stream = pa_stream_new(c, "Audio Capture", &ss, NULL);

            if (!pa->stream) {
                fprintf(stderr, "pa_stream_new() failed (capturing system audio).\n");
                pulse_quit(pa, 1);
                return;
            }

            pa_stream_set_read_callback(pa->stream, stream_read_callback, pa);

            if (pa_stream_connect_record(pa->stream, NULL, NULL, PA_STREAM_ADJUST_LATENCY) < 0) {
                fprintf(stderr, "pa_stream_connect_record() failed: %s\n", pa_strerror(pa_context_errno(c)));
                pulse_quit(pa, 1);
                return;
            }
            break;

        case PA_CONTEXT_TERMINATED:
            printf("PulseAudio context terminated.\n");
            pulse_quit(pa, 0);
            break;

        case PA_CONTEXT_FAILED:
            printf("PulseAudio context failed.\n");
            pulse_quit(pa, 1);
            break;
    }
}

void sink_info_callback(pa_context *c, const pa_sink_info *info, int eol, void *userdata) {
    if (info) {
        float volume = (float) pa_cvolume_avg(&(info->volume)) / (float)PA_VOLUME_NORM;
        printf("Percent volume = %.0f%%%s\n", volume * 100.0f, info->mute ? " (muted)" : "");
    }
}

void server_info_callback(pa_context *c, const pa_server_info *i, void *userdata) {
    printf("Default sink name = %s\n", i->default_sink_name);
    pa_context_get_sink_info_by_name(c, i->default_sink_name, sink_info_callback, userdata);
}

void subscribe_callback(pa_context *c, pa_subscription_event_type_t type, uint32_t idx, void *userdata) {
    unsigned facility = type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;

    pa_operation *op = NULL;

    switch (facility) {
        case PA_SUBSCRIPTION_EVENT_SINK:
            op = pa_context_get_sink_info_by_index(c, idx, sink_info_callback, userdata);
            break;

        default:
            assert(0); // unexpected event
            break;
    }

    if (op)
        pa_operation_unref(op);
}


// reads the system audio stream data (from PulseAudio)
void stream_read_callback(pa_stream *s, size_t length, void *userdata) {
    const void *data;

    if (pa_stream_peek(s, &data, &length) < 0) {
        fprintf(stderr, "pa_stream_peek() failed: %s\n", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
        return;
    }

    if (data) {
        const uint8_t *waveform = (const uint8_t *)data;

        if (length >= 1024) {

            struct timespec currentRenderTime;
            clock_gettime(CLOCK_MONOTONIC, &currentRenderTime);

            // if lastRenderTime is not set, initialize it
            if (lastRenderTime.tv_sec == 0 && lastRenderTime.tv_nsec == 0) {
                lastRenderTime = currentRenderTime;
            }

            // calculate elapsed time in nanoseconds
            long elapsedNs = (currentRenderTime.tv_sec - lastRenderTime.tv_sec) * 1000000000L +
                            (currentRenderTime.tv_nsec - lastRenderTime.tv_nsec);

            // render interval (16ms = 16,000,000 nanoseconds) -> 60 FPS
            // made it a lil slower
            const long renderIntervalNs = 16000000L;

            // check if enough time has passed since the last render
            if (elapsedNs >= renderIntervalNs) {
                // update the last render time to limit
                lastRenderTime = currentRenderTime;

                uint8_t spectrum[length / 2];
                memset(spectrum, 0, sizeof(spectrum));

                // calculate the spectrum
                calculate_spectrum(waveform, length, spectrum);

                size_t spectrumLength = sizeof(spectrum);

                float *presetsBuffer = NULL;  
                size_t sampleRate = 44100;
                size_t bitDepth = 32;
                size_t currentTime = performance_now();

                // render the audio visualization onto the current write buffer
                render(
                    // write to the current write buffer
                    frameBuffers[currentWriteBuffer].data, 
                    WIDTH,
                    HEIGHT,
                    waveform,
                    spectrum,
                    length,
                    spectrumLength,
                    bitDepth,
                    presetsBuffer,
                    0.0123f,
                    currentTime,
                    sampleRate
                );

                // double buffering: mark the buffer as ready and signal the rendering thread
                pthread_mutex_lock(&bufferMutex);

                frameBuffers[currentWriteBuffer].ready = 1;

                pthread_cond_signal(&bufferCond);

                // switch to the next write buffer
                currentWriteBuffer = (currentWriteBuffer + 1) % BUFFER_COUNT;

                pthread_mutex_unlock(&bufferMutex);
            }  
        }
    }

    pa_stream_drop(s); // free the buffer
}


// calculate the spectrum using KISS FFT
void calculate_spectrum(const uint8_t *waveform, size_t sample_count, uint8_t *spectrum) {
    // allocate memory for KISS FFT input and output
    kiss_fft_cpx in[sample_count];
    kiss_fft_cpx out[sample_count];
    kiss_fft_cfg cfg;

    // normalize waveform into input for FFT (convert uint8_t [0-255] to float [-1.0, 1.0])
    for (size_t i = 0; i < sample_count; i++) {
        // center around 0
        in[i].r = ((float)(waveform[i]) - 128.0f) / 128.0f; 
        in[i].i = 0.0f; // imaginary part is zero for real input
    }

    // Allocate FFT configuration
    cfg = kiss_fft_alloc(sample_count, 0, NULL, NULL);
    if (!cfg) {
        fprintf(stderr, "Failed to allocate KISS FFT configuration.\n");
        return;
    }

    // perform the FFT
    kiss_fft(cfg, in, out);

    // compute the magnitude of each frequency bin and normalize to [0, 255]
    // only first half is meaningful for real FFT
    for (size_t i = 0; i < sample_count / 2; i++) { 
        // compute magnitude
        float magnitude = sqrtf(out[i].r * out[i].r + out[i].i * out[i].i); 
        // normalize
        spectrum[i] = (uint8_t)(255.0f * magnitude / (float)sample_count); 
    }
    // free the FFT configuration
    free(cfg);
}

void initialize_glfw() {
    initialize_timer();
}

void cleanup_glfw() {
    // cleanup both frame buffers
    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (frameBuffers[i].data) {
            free(frameBuffers[i].data);
            frameBuffers[i].data = NULL;
        }
    }
}

// initialize frame buffers (called from main thread)
void initialize_frame_buffers() {
    for (int i = 0; i < BUFFER_COUNT; i++) {
        frameBuffers[i].data = (uint8_t *)malloc(WIDTH * HEIGHT * 4);
        if (!frameBuffers[i].data) {
            fprintf(stderr, "Failed to allocate memory for frame buffer %d.\n", i);
            cleanup_glfw();
            exit(EXIT_FAILURE);
        }
        memset(frameBuffers[i].data, 0, WIDTH * HEIGHT * 4); // initialize to black
        frameBuffers[i].ready = 0;
    }
}

// run function manages thread creation and termination
int run() {
    printf("Testing PulseAudio lib integration...\n");

    PulseAudio pa = {0};

    if (!pulse_initialize(&pa)){
        printf("Failed to initialize PulseAudio.\n");
        return 0;
    }

    // initialize frame buffers
    initialize_frame_buffers();

    // Cceate the rendering thread
    if (pthread_create(&renderThread, NULL, render_thread_func, (void *)&pa) != 0) {
        fprintf(stderr, "Failed to create rendering thread.\n");
        pulse_destroy(&pa);
        cleanup_glfw();
        exit(EXIT_FAILURE);
    }

    // create a separate thread for PulseAudio mainloop
    pthread_t pulseThread;
    if (pthread_create(&pulseThread, NULL, pulse_thread_func, &pa) != 0) {
        fprintf(stderr, "Failed to create PulseAudio thread.\n");
        pthread_cond_signal(&bufferCond);
        pthread_join(renderThread, NULL);
        pulse_destroy(&pa);
        cleanup_glfw();
        exit(EXIT_FAILURE);
    }

    // Wwit for PulseAudio thread to finish
    void *pulseRet;
    pthread_join(pulseThread, &pulseRet);

    // wake up the rendering thread if waiting
    pthread_cond_signal(&bufferCond); 
    pthread_join(renderThread, NULL);

    // cleanup PulseAudio resources
    pulse_destroy(&pa);

    // cleanup frame buffers
    cleanup_glfw();

    return (int)(intptr_t)pulseRet;
}
