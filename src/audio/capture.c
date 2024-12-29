#include "capture.h" 

// current rendered frame (buffer allocated globally)
uint8_t *frame = NULL;

// GLFW window and OpenGL texture
static GLFWwindow *window = NULL;
static GLuint texture;

#define WIDTH 1920
#define HEIGHT 1080

// SDL window
//SDL_Window *window = NULL;          

// SDL renderer
//SDL_Renderer *renderer = NULL;     

// global variable to store the reference time
static struct timeval app_start_time;

void initialize_timer() {
    gettimeofday(&app_start_time, NULL);
}

float performance_now() {
   struct timeval current_time;
    gettimeofday(&current_time, NULL);

    // Calculate elapsed time in microseconds
    uint64_t elapsed_usec =
        (current_time.tv_sec - app_start_time.tv_sec) * 1000000ULL +
        (current_time.tv_usec - app_start_time.tv_usec);

    // Convert elapsed time to milliseconds as double and return as float
    return (float)(elapsed_usec / 1000.0);
} 

// for when the window is resized, we want to resize the viewport
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}  

GLuint VAO, VBO, EBO;

void setup_vertex_data() {
    float vertices[] = {
        // Positions      // Texture Coords
        -1.0f, -1.0f,     0.0f, 0.0f,
         1.0f, -1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f,     0.0f, 1.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}


const char* vertex_shader_src = 
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"   // Input vertex position
    "layout(location = 1) in vec2 aTexCoord;\n" // Input texture coordinates
    "out vec2 TexCoord;\n"                 // Output texture coordinates
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
"in vec2 TexCoord;\n"
"out vec4 FragColor;\n"
"uniform sampler2D texture1;\n"
"uniform float vignetteIntensity; // Intensity of the vignette effect\n"
"uniform float zoomFactor;        // Zoom effect multiplier\n"
"uniform vec2 center;             // Dynamic center point\n"
"uniform float time;              // Time for animation\n"
"uniform float rotationSpeed;     // Speed of rotation\n"
"uniform float blurStrength;      // Strength of radial blur\n"
"uniform vec3 replacementColor;     // Most frequent color\n"
"\n"
// Radial blur helper function\n
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
"    // Rotate the texture coordinates\n"
"    float angle = time * rotationSpeed;\n"
"    mat2 rotationMatrix = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));\n"
"    vec2 rotatedUV = rotationMatrix * (TexCoord - center) + center;\n"
"\n"
"    // Apply zoom factor\n"
"    vec2 zoomUV = center + (rotatedUV - center) * zoomFactor;\n"
"\n"
"    // Add radial blur\n"
"    vec4 blurred = radialBlur(texture1, zoomUV, center, blurStrength);\n"
"\n"
"   // Replace black pixels with the most occurring color\n"
"   /*if (blurred.rgb == vec3(0.0, 0.0, 0.0)) {\n"
"        blurred.rgb = replacementColor;\n"
"    }*/\n"
"\n"
"    // Center dim effect\n"
"    vec2 offset = TexCoord - center;\n"
"    float distance = length(offset);\n"
"    float smoothValue = smoothstep(0.0, 0.9, distance);\n"
"    float dimFactor = mix(1.0 - 0.3, 1.0, smoothValue);\n"
"    dimFactor = clamp(dimFactor, 0.0, 1.0);\n"
"    blurred.rgb *= clamp(dimFactor, 0.0, 1.0);\n"
"\n"
"    // Gamma correction\n"
"    float gamma = 1.8; // Standard gamma correction\n"
"    blurred.rgb = pow(blurred.rgb, vec3(1.0 / gamma));\n"
"\n"
"    // Clamp the final color values to valid range\n"
"    blurred.rgb = clamp(blurred.rgb, 0.0, 1.0);\n"
"\n"
"    // Output the final color\n"
"    FragColor = blurred;\n"
"}\n";


GLuint shader_program;

void compile_and_link_shaders() {
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_src, NULL);
    glCompileShader(vertex_shader);

    GLint success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
        fprintf(stderr, "ERROR::VERTEX_SHADER_COMPILATION_FAILED\n%s\n", info_log);
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_src, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
        fprintf(stderr, "ERROR::FRAGMENT_SHADER_COMPILATION_FAILED\n%s\n", info_log);
    }

    shader_program = glCreateProgram();
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
}

// creates a singleton window instance and initializes the GPU primitives (OpenGL)
void initialize_glfw() {
    if (!window) {

        initialize_timer();

        if (!glfwInit()) {
            fprintf(stderr, "Failed to initialize GLFW.\n");
            exit(EXIT_FAILURE);
        }

        window = glfwCreateWindow(WIDTH, HEIGHT, "MilkyLinuxOSD", NULL, NULL);
        if (!window) {
            fprintf(stderr, "Failed to create GLFW window.\n");
            glfwTerminate();
            exit(EXIT_FAILURE);
        }

        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

        glfwMakeContextCurrent(window);

        if (glewInit() != GLEW_OK) {
            fprintf(stderr, "Failed to initialize GLEW.\n");
            exit(EXIT_FAILURE);
        }

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        compile_and_link_shaders();
        setup_vertex_data();
    }

    if (!frame) {
        size_t frameSize = WIDTH * HEIGHT * 4; // RGBA format
        frame = malloc(frameSize);
        if (!frame) {
            fprintf(stderr, "Failed to allocate framebuffer memory.\n");
            glfwDestroyWindow(window);
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        memset(frame, 0, frameSize); // Initialize to black
    }

}

void cleanup_glfw() {
    if (frame) {
        free(frame);
        frame = NULL;
    }

    if (texture) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }

    if (window) {
        glfwDestroyWindow(window);
        window = NULL;
        glfwTerminate();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shader_program);
}

ColorCount colorCounts[MAX_COLORS];
int colorCountSize = 0;

// Compare two colors
int color_equals(Color c1, Color c2) {
    return c1.r == c2.r && c1.g == c2.g && c1.b == c2.b;
}

// Add a color to the histogram
void add_color(Color c) {
    // Skip black
    if (c.r == 0 && c.g == 0 && c.b == 0) {
        return;
    }

    // Check if the color is already in the histogram
    for (int i = 0; i < colorCountSize; i++) {
        if (color_equals(colorCounts[i].color, c)) {
            colorCounts[i].count++;
            return;
        }
    }

    // Add a new color if not already present
    if (colorCountSize < MAX_COLORS) {
        colorCounts[colorCountSize].color = c;
        colorCounts[colorCountSize].count = 1;
        colorCountSize++;
    }
}

// Find the most frequent color
Color find_most_frequent_color() {
    Color mostFrequentColor = {0, 0, 0};
    int maxCount = 0;

    for (int i = 0; i < colorCountSize; i++) {
        if (colorCounts[i].count > maxCount) {
            maxCount = colorCounts[i].count;
            mostFrequentColor = colorCounts[i].color;
        }
    }

    return mostFrequentColor;
}


// Analyze the first pixel line of the framebuffer
Color analyze_first_line(uint8_t *frame, int width) {
    int numPixels = width > MAX_PIXELS ? MAX_PIXELS : width;

    for (int i = 0; i < numPixels * 4; i += 4) { // Step by 4 for RGBA
        Color c = {frame[i], frame[i + 1], frame[i + 2]}; // Extract RGB
        add_color(c);
    }

    return find_most_frequent_color();
}

// renders the frame (buffer) onto the 2D texture (OpenGL) inside of the window
static void render_frame(uint8_t *frame) {
    float time = glfwGetTime(); // Get elapsed time
    glUniform1f(glGetUniformLocation(shader_program, "time"), time);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame);


    

    // Update the "curvatureStrength" uniform
    float curvatureStrength = 0.05f; // Subtle curvature
    glUniform1f(glGetUniformLocation(shader_program, "curvatureStrength"), curvatureStrength);


    // Update the "grainAmount" uniform
    float vignetteIntensity = 1.1f; // Example grain amount
    glUniform1f(glGetUniformLocation(shader_program, "vignetteIntensity"), vignetteIntensity);

    /*
    Color mostFrequentColor = analyze_first_line(frame, 1000);
    float r = mostFrequentColor.r / 255.0f;
    float g = mostFrequentColor.g / 255.0f;
    float b = mostFrequentColor.b / 255.0f;

    GLint replacementColorLoc = glGetUniformLocation(shader_program, "replacementColor");
    glUniform3f(replacementColorLoc, r, g, b);*/

    // Update the "zoomFactor" uniform
    float zoomFactor = 0.995f; // Slight zoom-out
    glUniform1f(glGetUniformLocation(shader_program, "zoomFactor"), zoomFactor);

    // Update the "grainAmount" uniform
    float grainAmount = 0.03f; // Example grain amount
    glUniform1f(glGetUniformLocation(shader_program, "grainAmount"), grainAmount);

    // Update the "center" uniform (dynamic movement over time)
    float centerX = 0.5f; /*+ 0.1f * sin(time * 0.5f); */// Moves left and right
    float centerY = 0.5f; /*+ 0.1f * cos(time * 0.3f);*/ // Moves up and down
    glUniform2f(glGetUniformLocation(shader_program, "center"), centerX, centerY);

    // Update the "rotationSpeed" uniform
    float rotationSpeed = 0.0f; // Example rotation speed (radians per second)
    glUniform1f(glGetUniformLocation(shader_program, "rotationSpeed"), rotationSpeed);

    // Update the "blurStrength" uniform
    float blurStrength = 0.001f; // Subtle radial blur
    glUniform1f(glGetUniformLocation(shader_program, "blurStrength"), blurStrength);


    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shader_program);

    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    if (!glfwWindowShouldClose(window)) {
        glfwSwapBuffers(window);
        glfwPollEvents();
    } else {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
}


// the struct is passed by reference (pointer into memory)
int pulse_initialize(PulseAudio *pa) {

    // allocate a new PulseAudio mainloop
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

    // allocate a signal for exiting (SIGNAL INTERRUPT, see POSIX standards)
    pa->signal = pa_signal_new(SIGINT, exit_signal_callback, pa);
    if (!pa->signal) {
        fprintf(stderr, "pa_signal_new() failed\n");
        return 0;
    }

    // we accept signals for a broken pipe (broken audio stream) and signal for ignore states
    signal(SIGPIPE, SIG_IGN);

    // allocate a new context for the mainloop api with name
    pa->context = pa_context_new(pa->mainloop_api, "Milky PulseAudio Test");
    if (!pa->context) {
        fprintf(stderr, "pa_context_new() failed\n");
        return 0;
    }

    // we try to connect to PulseAudio and let "our" mainloop run
    if (pa_context_connect(pa->context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
        fprintf(stderr, "pa_context_connect() failed: %s\n", pa_strerror(pa_context_errno(pa->context)));
        return 0;
    }

    // whenever a state in context changes, we want PulseAudio to call our state callback!
    pa_context_set_state_callback(pa->context, context_state_callback, pa);

    // create window and rendering surface
    //initialize_sdl_resources(1440, 900);

    return 1;
} 

void exit_signal_callback(pa_mainloop_api *m, pa_signal_event *e, int sig, void *userdata) {
    PulseAudio *pa = (PulseAudio *)userdata;
    printf("Exit signal (free resources here for the moment!)");
    if (pa) pulse_quit(pa, 0);

    //cleanup_sdl_resources();
}

void sink_info_callback(pa_context *c, const pa_sink_info *info, int eol, void *userdata) {
    if (info) {
        float volume = (float) pa_cvolume_avg(&(info->volume)) / (float)PA_VOLUME_NORM;
        printf("percent volume = %.0f%%%s\n", volume * 100.0f, info->mute ? " (muted)" : "");
    }   
}

void server_info_callback(pa_context *c, const pa_server_info *i, void *userdata) {
    printf("default sink name = %s\n", i->default_sink_name); // let's see what we've got!
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
            assert(0); // Unexpected event
            break;
    }

    if (op)
        pa_operation_unref(op);
}

// Static variables to track time and rendering
static struct timespec lastRenderTime = {0, 0};

// reads the system audio stream data
void stream_read_callback(pa_stream *s, size_t length, void *userdata) {
    const void *data;

/*
        printf("stop = %d\n", milky_sig_stop);
        printf("Address of milky_sig_stop: %p\n", (void*)&milky_sig_stop);

    if (milky_sig_stop) {
        printf("stop = %d\n", milky_sig_stop);
        printf("\nCtrl+C detected. Skipping...\n");  
        return;  
    }  



    if (!milky_sig_stop) {
    */
        
    if (pa_stream_peek(s, &data, &length) < 0) {
        fprintf(stderr, "pa_stream_peek() failed: %s\n", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
    }

    initialize_glfw();


    if (data) {
        const uint8_t *waveform = (const uint8_t *)data;

        // the previous visualization fits perfect for the old monitor, but it's not that cool
        // for the on-screen display (music visualization - as it is distorted)
        // the reason probably is that we're receiving "wrong" data from PulseAudio
        // need to analyze PulseAudio data now. Let me see if I can render the 
        // visualization on top once it appears.

        // PulseAudio delivers 4-byte metadata chunks that we'd need to ignore
        if (length >= 1024) {

            struct timespec currentRenderTime;
            clock_gettime(CLOCK_MONOTONIC, &currentRenderTime);

            // If lastRenderTime is not set, initialize it
            if (lastRenderTime.tv_sec == 0 && lastRenderTime.tv_nsec == 0) {
                lastRenderTime = currentRenderTime;
            }

            // Calculate elapsed time in nanoseconds
            long elapsedNs = (currentRenderTime.tv_sec - lastRenderTime.tv_sec) * 1000000000L +
                            (currentRenderTime.tv_nsec - lastRenderTime.tv_nsec);

            // Define the render interval (33ms = 33,000,000 nanoseconds)
            const long renderIntervalNs = 24000000L;

            // Check if enough time has passed since the last render
            if (elapsedNs >= renderIntervalNs) {
                // Update the last render time
                lastRenderTime = currentRenderTime;

                // here we need to call an FFT algorithm (or implement one)
                // so that we can get the frequency spectrum for the waveform
                // this is necessary to calculate the spectral flux    
                //printf("Received waveform data of size: %zu\n", length);

                uint8_t spectrum[length / 2];

                // specifically initialize memory with zeros
                memset(spectrum, 0, sizeof(spectrum));

                // calculate the spectrum
                calculate_spectrum(waveform, length, spectrum);

                size_t spectrumLength = sizeof(spectrum);

                // We're good here.
                //printf("Received FFT spectrum data of size: %zu\n", spectrumLength);

                // cool! now we only need to call the VIDEO function
                // and render the framebuffer onto a surface that can handle
                // framebuffer data -- you'll then see the current playback sound
                // being beautifully visualized!
                // We'll use SDL to render the framebuffer on a new window that we're 
                // going to create.

                // simple demo: width * height * count of values we have to reserve 
                // memory for because a framebuffer wants Red, Green, Blue, Alpha 
                // cannel values per pixel.

                size_t frameSize = WIDTH * HEIGHT * 4; // RGBA

                // if frame buffer isn't allocated yet, do it once during initialization
                if (frame == NULL) {
                    // allocate memory once
                    frame = (uint8_t *)malloc(frameSize); 
                    if (!frame) {
                        fprintf(stderr, "Failed to allocate memory for the frame buffer.\n");
                    }
                }

                memset(frame, 0, frameSize);

                float *presetsBuffer = NULL; 
                float speed = 0.1899f;
                size_t sampleRate = 44100;
                size_t bitDepth = 32;
                size_t currentTime = performance_now();
                
                // rendering the audio 
                render(
                    frame,
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


                // Render the frame
                render_frame(frame);
            }
            

        }
    }
    pa_stream_drop(s); // free 
    
    //}   
}  

// handles PulseAudio context state changes
void context_state_callback(pa_context *c, void *userdata) {

    assert(c && userdata);

    PulseAudio *pa = (PulseAudio *)userdata;

    // depending on the state change, we need to handle in different ways...
    switch (pa_context_get_state(c)) {

        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
            fprintf(stderr, "PulseAudio connection established.\n");
            pa_context_get_server_info(c, server_info_callback, userdata);

            // Subscribe to sink events for volume change notifications
            pa_context_set_subscribe_callback(c, subscribe_callback, userdata);
            pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SINK, NULL, NULL);

            // Let's try to record system audio (the audio you're hearing right now!)
            pa_sample_spec ss = {
                .format = PA_SAMPLE_U8, // 16-bit little-endian
                .rate = 44100,             // 44.1kHz sample rate
                .channels = 2              // Stereo
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
            printf("Well, we've got kicked out...\n");
             pulse_quit(pa, 0);
            break;

        case PA_CONTEXT_FAILED:
            printf("Well, shit got us good.. nothing works!\n");
             pulse_quit(pa, 1);
            break;
    }  
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

int pulse_run(PulseAudio *pa) {

    int ret = 1;

    // TODO: might either implement this iteratively 

    /**
     * while (!stop_requested) {
        if (pa_mainloop_iterate(pa->mainloop, 1, &ret) < 0) {
            fprintf(stderr, "pa_mainloop_iterate() failed\n");
            return ret;
        }
    }
     */

    // TODO: or use pthread to handle SIGINT on the main thread and pulseaudio on a second one

    if (pa_mainloop_run(pa->mainloop, &ret) < 0) {
        fprintf(stderr, "pa_mainloop_run() failed.\n");
        return ret;
    }
    return ret;
} 

void pulse_quit(PulseAudio *pa, int ret) {
    pa->mainloop_api->quit(pa->mainloop_api, ret);
}

int run() {

    printf("Testing PulseAudio lib integration...");

    PulseAudio pa = {0};

    if (!pulse_initialize(&pa)){
        printf("Nah, lets go sleep now.. this is tiring!!");
        return 0;
    }

    int ret = pulse_run(&pa);

    pulse_destroy(&pa);

    return ret;
}  


void calculate_spectrum(const uint8_t *waveform, size_t sample_count, uint8_t *spectrum) {
    // Step 1: Allocate memory for KISS FFT input and output
    kiss_fft_cpx in[sample_count];
    kiss_fft_cpx out[sample_count];
    kiss_fft_cfg cfg;

    // Step 2: Normalize waveform into input for FFT (convert uint8_t [0-255] to float [-1.0, 1.0])
    for (size_t i = 0; i < sample_count; i++) {

        // Center around 0
        in[i].r = ((float)(waveform[i]) - 128.0f) / 128.0f; 

        // Imaginary part is zero for real input
        in[i].i = 0.0f; 
    }

    // Step 3: Allocate FFT configuration
    cfg = kiss_fft_alloc(sample_count, 0, NULL, NULL);
    if (!cfg) {
        fprintf(stderr, "Failed to allocate KISS FFT configuration.\n");
        return;
    }

    // Step 4: Perform the FFT
    kiss_fft(cfg, in, out);

    // Step 5: Compute the magnitude of each frequency bin and normalize to [0, 255]
    for (size_t i = 0; i < sample_count / 2; i++) { // Only first half is meaningful for real FFT

        // Compute magnitude
        float magnitude = sqrtf(out[i].r * out[i].r + out[i].i * out[i].i); 

        // Normalization
        spectrum[i] = (uint8_t)(255.0f * magnitude / (float)sample_count); 
    }

    // Step 6: Free the FFT configuration
    free(cfg);
}

// TODO: need to refactor this. Rendering does NOT belong here (in audio capture code ;)
// need to pass a function pointer and do all that in a callback function
// defined in main.c (which does not exist yet ;)
/*
int initialize_sdl_resources(size_t canvasWidthPx, size_t canvasHeightPx) {
    if (!window && !renderer) {

        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
            return -1;
        }

        window = SDL_CreateWindow("Visualizer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  canvasWidthPx, canvasHeightPx, SDL_WINDOW_SHOWN);
        if (!window) {
            fprintf(stderr, "Failed to create SDL window: %s\n", SDL_GetError());
            return -1;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            fprintf(stderr, "Failed to create SDL renderer: %s\n", SDL_GetError());
            return -1;
        }

        size_t frameSize = canvasWidthPx * canvasHeightPx * 4; // RGBA
        frame = (uint8_t *)malloc(frameSize);
        if (!frame) {
            fprintf(stderr, "Failed to allocate memory for the frame buffer.\n");
            return -1;
        }
    }
    return 0;
}

// we don't want to leak memory, so we've gotta free() the resources
void cleanup_sdl_resources() {
    if (frame) {
        free(frame);
        frame = NULL;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
}*/