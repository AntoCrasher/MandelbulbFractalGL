#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>

using namespace std;

struct vec3 {
    float x;
    float y;
    float z;
};

vec3 cross(const vec3& a, const vec3& b) {
    vec3 result;
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
    return result;
}
float length(const vec3& a) {
    return sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}
vec3 normalize(const vec3& a) {
    float len = length(a);
    return { a.x / len, a.y / len, a.z / len };
}

const int MAX_FRAMES = 2000;

const int FRAME_WIDTH = 1000;
const int FRAME_HEIGHT = 1000;

const int PIXEL_BUFFER_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 3;

const bool SAVE_FRAMES = false;

float mouse_x = 0.0f;
float mouse_y = 0.0f;
float mouse_scroll = 0.0f;

vec3 cameraPosition = { 0.0f, 0.0f, -2.0f };
vec3 cameraForward = { 0.0f, 0.0f, 1.0f };

float fov = 60.0f; 
float zoom_speed = 0.5f;

float cameraYaw = -90.0f;
float cameraPitch = 0.0f;

float lastMouseX = 0.0f;
float lastMouseY = 0.0f;

float cameraSpeed = 0.03f;
float cameraSpeedChange = 0.0001f;

bool firstMouse = true;

struct ShaderProgramSource {
    string VertexSource;
    string FragmentSource;
};
static ShaderProgramSource ParseShader(const string& filepath)
{
    ifstream stream(filepath);

    enum class ShaderType {
        NONE = -1,
        VERTEX = 0,
        FRAGMENT = 1
    };

    string line;
    stringstream ss[2];
    ShaderType type = ShaderType::NONE;

    while (getline(stream, line))
    {
        if (line.find("#shader") != string::npos)
        {
            if (line.find("vertex") != string::npos)
                type = ShaderType::VERTEX;
            else if (line.find("fragment") != string::npos)
                type = ShaderType::FRAGMENT;
        }
        else
        {
            ss[(int)type] << line << '\n';
        }
    }
    return { ss[0].str(), ss[1].str() };
}

static unsigned int CompileShader(unsigned int type, const string& source)
{
    unsigned int id = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE)
    {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        char* message = (char*)alloca(length * sizeof(char));
        glGetShaderInfoLog(id, length, &length, message);
        cout << "FAILED TO COMPILE " << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") << " SHADER!" << endl;
        cout << message << endl;
        glDeleteShader(id);
        return 0;
    }

    return id;
}
static unsigned int CreateShader(const string& vertexShader, const string& fragmentShader)
{
    unsigned int program = glCreateProgram();
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glValidateProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

void save_bitmap(const string& filename, GLubyte* imageData)
{
    // Define the bitmap file header
    unsigned char bitmapFileHeader[14] = {
            'B', 'M',                     // Signature
            0, 0, 0, 0,           // File size (to be filled later)
            0, 0, 0, 0,           // Reserved
            54, 0, 0, 0        // Pixel data offset
    };

    // Define the bitmap info header
    unsigned char bitmapInfoHeader[40] = {
            40, 0, 0, 0,            // Info header size
            0, 0, 0, 0,             // Image width (to be filled later)
            0, 0, 0, 0,           // Image height (to be filled later)
            1, 0,                         // Number of color planes
            24, 0,                        // Bits per pixel (24 bits for RGB)
            0, 0, 0, 0,          // Compression method (none)
            0, 0, 0, 0,          // Image size (can be set to 0 for uncompressed images)
            0, 0, 0, 0,          // Horizontal resolution (can be set to 0 for uncompressed images)
            0, 0, 0, 0,          // Vertical resolution (can be set to 0 for uncompressed images)
            0, 0, 0, 0,          // Number of colors in the palette (not used for 24-bit images)
            0, 0, 0, 0           // Number of important colors (not used for 24-bit images)
    };

    // Calculate the padding bytes
    int paddingSize = (4 - (FRAME_WIDTH * 3) % 4) % 4;

    // Calculate the file size
    int fileSize = 54 + (FRAME_WIDTH * FRAME_HEIGHT * 3) + (paddingSize * FRAME_HEIGHT);

    // Fill in the file size in the bitmap file header
    bitmapFileHeader[2] = (unsigned char)(fileSize);
    bitmapFileHeader[3] = (unsigned char)(fileSize >> 8);
    bitmapFileHeader[4] = (unsigned char)(fileSize >> 16);
    bitmapFileHeader[5] = (unsigned char)(fileSize >> 24);

    // Fill in the image width in the bitmap info header
    bitmapInfoHeader[4] = (unsigned char)(FRAME_WIDTH);
    bitmapInfoHeader[5] = (unsigned char)(FRAME_WIDTH >> 8);
    bitmapInfoHeader[6] = (unsigned char)(FRAME_WIDTH >> 16);
    bitmapInfoHeader[7] = (unsigned char)(FRAME_WIDTH >> 24);

    // Fill in the image height in the bitmap info header
    bitmapInfoHeader[8] = (unsigned char)(FRAME_HEIGHT);
    bitmapInfoHeader[9] = (unsigned char)(FRAME_HEIGHT >> 8);
    bitmapInfoHeader[10] = (unsigned char)(FRAME_HEIGHT >> 16);
    bitmapInfoHeader[11] = (unsigned char)(FRAME_HEIGHT >> 24);

    // Open the output file
    ofstream file(filename, ios::binary);

    // Write the bitmap headers
    file.write(reinterpret_cast<const char*>(bitmapFileHeader), sizeof(bitmapFileHeader));
    file.write(reinterpret_cast<const char*>(bitmapInfoHeader), sizeof(bitmapInfoHeader));

    // Write the pixel data (BGR format) row by row
    for (int y = FRAME_HEIGHT - 1; y >= 0; y--)
    {
        for (int x = 0; x < FRAME_WIDTH; x++)
        {
            // Calculate the pixel position
            int position = (x + y * FRAME_WIDTH) * 3;

            // Write the pixel data (BGR order)
            file.write(reinterpret_cast<const char*>(&imageData[position + 2]), 1); // Blue
            file.write(reinterpret_cast<const char*>(&imageData[position + 1]), 1); // Green
            file.write(reinterpret_cast<const char*>(&imageData[position]), 1);     // Red
        }

        // Write the padding bytes
        for (int i = 0; i < paddingSize; i++)
        {
            file.write("\0", 1);
        }
    }

    // Close the file
    file.close();
}
void save_frame(const string& filename, GLubyte* pixels)
{
    // Flip the frame vertically if needed (OpenGL stores pixels from bottom to top)
    for (int y = 0; y < FRAME_HEIGHT / 2; ++y) {
        for (int x = 0; x < FRAME_WIDTH; ++x) {
            for (int c = 0; c < 3; ++c) {
                swap(pixels[(y * FRAME_WIDTH + x) * 3 + c], pixels[((FRAME_HEIGHT - 1 - y) * FRAME_WIDTH + x) * 3 + c]);
            }
        }
    }

    // Save the frame to a file
    save_bitmap(filename, pixels);

    // Release the pixel data
    delete[] pixels;
}

string sec_to_time(float time) 
{
    float n_time = time;
    string suffix = " second(s)";
    if (n_time > 60.0f * 60.0f * 24.0f)
    {
        n_time /= 60.0f * 60.0f * 24.0f;
        suffix = " day(s)";
    }
    else if (n_time > 60.0f * 60.0f)
    {
        n_time /= 60.0f * 60.0f;
        suffix = " hour(s)";
    }
    else if (n_time > 60.0f)
    {
        n_time /= 60.0f;
        suffix = " minute(s)";
    } 
    

    return to_string(n_time) + suffix;
}

float radians(float deg) {
    return deg * (3.141f / 180.0f);
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    mouse_x = (float)xpos;
    mouse_y = (float)ypos;

    if (firstMouse)
    {
        lastMouseX = xpos;
        lastMouseY = ypos;
        firstMouse = false;
    }

    float xOffset = xpos - lastMouseX;
    float yOffset = lastMouseY - ypos; // rev   ersed since y-coordinates range from bottom to top

    lastMouseX = xpos;
    lastMouseY = ypos;

    float sensitivity = 0.3f;
    xOffset *= sensitivity;
    yOffset *= sensitivity; 

    cameraYaw -= xOffset;
    cameraPitch -= yOffset;

    // Clamp pitch to prevent the camera from flipping upside down
    if (cameraPitch > 89.0f)
        cameraPitch = 89.0f;
    if (cameraPitch < -89.0f)
        cameraPitch = -89.0f;

    cameraForward.x = cos(radians(cameraPitch)) * sin(radians(cameraYaw));
    cameraForward.y = sin(radians(cameraPitch));
    cameraForward.z = cos(radians(cameraPitch)) * cos(radians(cameraYaw));

    cameraForward = normalize(cameraForward);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    mouse_scroll += (float)yoffset;
    mouse_scroll = max(mouse_scroll, 0.0f);
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    vec3 cameraRight = cross(cameraForward, { 0.0f, 1.0f, 0.0f });
    cameraRight = normalize(cameraRight);

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        cameraPosition.x += cameraForward.x * cameraSpeed;
        cameraPosition.y += cameraForward.y * cameraSpeed;
        cameraPosition.z += cameraForward.z * cameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        cameraPosition.x -= cameraForward.x * cameraSpeed;
        cameraPosition.y -= cameraForward.y * cameraSpeed;
        cameraPosition.z -= cameraForward.z * cameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        cameraPosition.x += cameraRight.x * cameraSpeed;
        cameraPosition.y += cameraRight.y * cameraSpeed;
        cameraPosition.z += cameraRight.z * cameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        cameraPosition.x -= cameraRight.x * cameraSpeed;
        cameraPosition.y -= cameraRight.y * cameraSpeed;
        cameraPosition.z -= cameraRight.z * cameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        cameraPosition.y += cameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        cameraPosition.y -= cameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
        fov -= zoom_speed;
    }
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
        fov += zoom_speed;
    }
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        cameraSpeed += cameraSpeedChange;
    }
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        cameraSpeed -= cameraSpeedChange;
    }
}

int main(void)
{
    GLFWwindow* window;

    // INITIALIZE THE LIBRARY
    if (!glfwInit())
        return -1;

    // CREATE A WINDOWED MODE WINDOW AND ITS OPENGL CONTEXT
    window = glfwCreateWindow(FRAME_WIDTH, FRAME_HEIGHT, "GLSL", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    // MAKE THE WINDOW'S CONTEXT CURRENT
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);

    glViewport(0, 0, FRAME_WIDTH, FRAME_HEIGHT);

    if (glewInit() != GLEW_OK)
        cout << "ERROR!" << endl;

    cout << glGetString(GL_VERSION) << endl;

    // CREATE TRIANGLE COORDS / BUFFERS
    float positions[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,

         1.0f,  1.0f,
        -1.0f,  1.0f,
        -1.0f, -1.0f
    };

    unsigned int buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), positions, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);

    // LOAD SHADER
    ShaderProgramSource source = ParseShader("res/shaders/Basic.frag");

    cout << "VERTEX" << endl;
    cout << source.VertexSource << endl;

    cout << "FRAGMENT" << endl;
    cout << source.FragmentSource << endl;
    
    // CREATE / USE SHADER
    unsigned int shader = CreateShader(source.VertexSource, source.FragmentSource);
    glUseProgram(shader);
    
    // INIT UNIFORM LOCATIONS
    int timeLocation = glGetUniformLocation(shader, "u_time");
    int mouseLocation = glGetUniformLocation(shader, "u_mouse");
    int resolutionLocation = glGetUniformLocation(shader, "u_resolution");
    int camposLocation = glGetUniformLocation(shader, "u_campos");
    int camdirLocation = glGetUniformLocation(shader, "u_camdir");
    int fovLocation = glGetUniformLocation(shader, "u_fov");

    // INIT FRAME BUFFER
    GLubyte** frame_buffer = new GLubyte*[MAX_FRAMES];

    chrono::system_clock::time_point start_time = chrono::system_clock::now();

    cout << "RENDERING FRAMES..." << endl;
    
    int start_frame = 0;
    int frame = start_frame;

    // LOOP UNTIL THE USER CLOSES THE WINDOW
    while (!glfwWindowShouldClose(window))
    {
        if (SAVE_FRAMES && frame >= MAX_FRAMES)
            break;

        chrono::system_clock::time_point start_frame = chrono::system_clock::now();

        processInput(window);

        // GET TIME
        float timeValue = (float)frame / (float)MAX_FRAMES * 3.141f * 2.0f / 0.132f;

        // UPDATE UNIFORM PARAMS
        glUniform1f(timeLocation, timeValue);
        glUniform3f(mouseLocation, mouse_x, mouse_y, mouse_scroll);
        glUniform2f(resolutionLocation, float(FRAME_WIDTH), float(FRAME_HEIGHT));

        glUniform3f(camposLocation, cameraPosition.x, cameraPosition.y, cameraPosition.z);
        glUniform3f(camdirLocation, cameraForward.x, cameraForward.y, cameraForward.z);

        glUniform1f(fovLocation, fov);

        cout << "FOV: " << fov << " Camera Speed: " << cameraSpeed << endl;

        // RENDER FRACTAL
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        if (SAVE_FRAMES) {
            // ADD PIXELS TO FRAME BUFFEER
            GLubyte* pixels = new GLubyte[PIXEL_BUFFER_SIZE];
            glReadPixels(0, 0, FRAME_WIDTH, FRAME_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, pixels);
            frame_buffer[frame] = pixels;
        }

        // SWAP FRONT AND BACK BUFFERS
        glfwSwapBuffers(window);

        // POLL FOR AND PROCESS EVENTS
        glfwPollEvents();

        if (SAVE_FRAMES) {
            // UPDATE PROGRESS
            chrono::time_point<chrono::system_clock> end_frame = chrono::system_clock::now();
            chrono::duration<float> duration_frame = end_frame - start_frame;
        
            cout << "RENDERED: " << frame + 1 << "/" << MAX_FRAMES << " (" << floor((float)(frame + 1.0f) / (float)MAX_FRAMES * 1000.0f) / 10.0f << "%)" << " " << sec_to_time(duration_frame.count()) << " | ETA: " << sec_to_time((float)(MAX_FRAMES - (frame + 1)) * duration_frame.count()) << endl;
        }
        frame++;
    }
    if (SAVE_FRAMES) {
        cout << "SAVING FRAMES..." << endl;

        // SAVE ALL FRAMES TO DISK
        for (int i = start_frame; i < frame + 1; i++) {
            chrono::system_clock::time_point start_frame = chrono::system_clock::now();
        
            // SAVE FRAME TO DISK
            save_frame("./output/frame_" + to_string(i) + ".bmp", frame_buffer[i]);

            // UPDATE PROGRESS
            chrono::time_point<chrono::system_clock> end_frame = chrono::system_clock::now();
            chrono::duration<float> duration_frame = end_frame - start_frame;

            cout << "SAVED: " << i + 1 << "/" << frame << " (" << floor((float)(i + 1.0f) / (float)frame * 1000.0f) / 10.0f << "%)" << " " << sec_to_time(duration_frame.count()) << "    | ETA: " << sec_to_time((float)(frame-(i + 1)) * duration_frame.count()) << endl;
        }

        chrono::time_point<chrono::system_clock> end_time = chrono::system_clock::now();
        chrono::duration<float> duration = end_time - start_time;
        cout << "Total time taken: " << sec_to_time(duration.count()) << endl;
    }
    // DELTE SHADER
    glDeleteProgram(shader);

    // CLEAR FRAMES BUFFER
    delete[] frame_buffer;

    // TERMINATE THE LIBRARY
    glfwTerminate();
    return 0;
}   
