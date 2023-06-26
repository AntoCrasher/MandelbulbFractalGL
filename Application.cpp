#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>

using namespace std;

const int MAX_FRAMES = 2000;

const int FRAME_WIDTH = 2000;
const int FRAME_HEIGHT = 2000;

const int PIXEL_BUFFER_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 3;

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
    
    // INIT PARAM U_TIME
    int timeLocation = glGetUniformLocation(shader, "u_time");

    // INIT FRAME BUFFER
    GLubyte** frame_buffer = new GLubyte*[MAX_FRAMES];

    chrono::system_clock::time_point start_time = chrono::system_clock::now();

    cout << "RENDERING FRAMES..." << endl;
    
    int start_frame = 0;
    int frame = start_frame;

    // LOOP UNTIL THE USER CLOSES THE WINDOW
    while (!glfwWindowShouldClose(window))
    {
        if (frame >= MAX_FRAMES)
            break;

        chrono::system_clock::time_point start_frame = chrono::system_clock::now();

        // GET TIME
        float timeValue = (float)frame / (float)MAX_FRAMES * 3.141f * 2.0f / 0.132f;
        glUniform1f(timeLocation, timeValue);

        // RENDER FRACTAL
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ADD PIXELS TO FRAME BUFFEER
        GLubyte* pixels = new GLubyte[PIXEL_BUFFER_SIZE];
        glReadPixels(0, 0, FRAME_WIDTH, FRAME_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        frame_buffer[frame] = pixels;

        // SWAP FRONT AND BACK BUFFERS
        glfwSwapBuffers(window);

        // POLL FOR AND PROCESS EVENTS
        glfwPollEvents();


        // UPDATE PROGRESS
        chrono::time_point<chrono::system_clock> end_frame = chrono::system_clock::now();
        chrono::duration<float> duration_frame = end_frame - start_frame;
        
        cout << "RENDERED: " << frame + 1 << "/" << MAX_FRAMES << " (" << floor((float)(frame + 1.0f) / (float)MAX_FRAMES * 1000.0f) / 10.0f << "%)" << " " << sec_to_time(duration_frame.count()) << " | ETA: " << sec_to_time((float)(MAX_FRAMES - (frame + 1)) * duration_frame.count()) << endl;
        frame++;
    }

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

    // DELTE SHADER
    glDeleteProgram(shader);

    // CLEAR FRAMES BUFFER
    delete[] frame_buffer;

    // TERMINATE THE LIBRARY
    glfwTerminate();
    return 0;
}
