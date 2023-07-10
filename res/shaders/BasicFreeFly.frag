#shader vertex
#version 330 core

layout(location = 0) in vec4 position;
out vec2 fragPosition;

void main()
{
    gl_Position = position;
    fragPosition = position.xy;
};

#shader fragment
#version 330 core

layout(location = 0) out vec4 color;
in vec2 fragPosition;

uniform float u_time;
uniform vec3 u_mouse;
uniform vec2 u_resolution;
uniform vec3 u_campos;
uniform vec3 u_camdir;
uniform float u_fov;

#define MAX_ITERS 500
#define MAX_ITERS_MARCH 500
#define EPSILON 0.0001
#define MAX_DISTANCE 100.0

#define TIME_SCALE 1.0
#define TIME_OFFSET 5.616

#define FOV u_fov

#define FOCAL_LENGTH 2.920
#define APERTURE 0.024
#define NUM_SAMPLES 50

#define COLOR_SCALE 0.007
#define COLOR_OFFSET 2.690

#define USE_DOF false

#define COLOR_A 0.500, 0.500, 0.500
#define COLOR_B 0.500, 0.500, 0.500
#define COLOR_C 1.000, 1.000, 1.000
#define COLOR_D 0.000, 1.058, 0.058

struct YawPitch {
    float yaw;
    float pitch;
};

// COLOR PALETTE
vec3 palette(float t) {
    vec3 a = vec3(COLOR_A);
    vec3 b = vec3(COLOR_B);
    vec3 c = vec3(COLOR_C);
    vec3 d = vec3(COLOR_D);
    return a + b*cos(6.28318*(c*t+d));
}

// MANDEL BULB SIGNED DISTANCE FUNCTION
float mandelbulb_distance(vec3 point) {
    vec3 z = point;
    float dr = 1.0;
    float r = 0.0;
    float max_pow = 11.640;
    float power = u_mouse.z / 50.0f + 1.1;//(((sin(u_time * 0.132 * TIME_SCALE + TIME_OFFSET) + 1.0) / 2.0) * max_pow) + 4.0;
    for (int i = 0; i < MAX_ITERS; i++) {
        r = length(z);
        if (r > 2.0)
            break;
        float theta = atan(z.y, z.x);
        float phi = acos(z.z / r);
        dr = pow(r, power - 1.0) * power * dr + 1.0;
        float zr = pow(r, power);
        theta = theta * power;
        phi = phi * power;
        z = vec3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi)) * zr + point;
    }
    return 0.5 * log(r) * r / dr;
}

// RAY MARCH FRACTAL TOWARDS DIRECTION
vec3 ray_march_fractal(vec3 origin, vec3 direction) {
    float dist = 0.0;
    float total_dist = 0.0;
    vec3 pos = origin;
    for (int i = 0; i < MAX_ITERS_MARCH; i++) {
        dist = mandelbulb_distance(pos);
        total_dist += dist;
        pos = origin + direction * total_dist;
        if (dist < EPSILON) {
            float s = (1.0 + sin(float(i) * COLOR_SCALE + COLOR_OFFSET)) / 2.0 * 2.296 + 2.216;
            float ao = pow((0.9 - max(float(i) / float(MAX_ITERS_MARCH), 0.0)), 3.800) + 0.5;
            return palette(s) * ao;
        }
        if (total_dist > MAX_DISTANCE) {
            break;
        }
    }
    return vec3(0.0, 0.0, 0.0);
}

// RANDOM 0-1 FROM SEED
float rand(vec2 seed)
{
    return fract(sin(dot(seed.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

// GET ANGLE AND AXIS FROM 2 VECTORS
vec2 calculate_yaw_pitch(vec3 v) {
  vec3 forward = normalize(v);
  float pitch = asin(-forward.y);
  float yaw = atan(forward.x, forward.z);
  return vec2(pitch, yaw);
}

// ROTATE VECTOR BY ANGLE AROUND AXIS
vec3 rotate_vector(vec3 vector, float pitch, float yaw) {
    float cosPitch = cos(pitch);
    float sinPitch = sin(pitch);
    float cosYaw = cos(yaw);
    float sinYaw = sin(yaw);
    vec3 rotatedVector = vec3(vector.x, (vector.y * cosPitch) - (vector.z * sinPitch), (vector.y * sinPitch) + (vector.z * cosPitch));
    rotatedVector = vec3((rotatedVector.x * cosYaw) + (rotatedVector.z * sinYaw), rotatedVector.y, (-rotatedVector.x * sinYaw) + (rotatedVector.z * cosYaw));
    return rotatedVector;
}

void main()
{
    // UV COORDS
    vec2 uv = fragPosition.xy;

    // INIT CAMERA PARAMS
    float size = max((u_mouse.y / u_resolution.y) * 10.0 - 5.0, 0.0) + 0.5;  //sin(u_time * TIME_SCALE + TIME_OFFSET) * 0.0 + FOCAL_LENGTH;
    float speed = 0.000;
    float cam_offset = (1.0 - u_mouse.x / u_resolution.x) * 2.0 * 3.141;
    float t = cam_offset; //-1.592 + sin(u_time * 0.132 * TIME_SCALE + TIME_OFFSET) * 0.8 + cam_offset;
    float look_size = 1.000;

    // TARGET CENTER / CAM POS
    vec3 center = vec3(0.0);//vec3(cos(-1.592) * look_size, 0.0, sin(-1.592) * look_size);
    vec3 cam_pos = u_campos;//vec3(size * cos(t), 0.0, size * sin(t)) + center;

    // GET FOV PIXEL COORDS
    float fovRad = FOV * (3.141 / 180.0);
    float tanHalfFov = tan(fovRad * 0.5);
    float ny = tanHalfFov * uv.y;
    float nx = tanHalfFov * uv.x;

    // GET DIRECTION
    vec3 direction = normalize(vec3(nx, ny, -1.0));

    vec2 py = calculate_yaw_pitch(u_camdir);
    direction = rotate_vector(direction, py.x, py.y);

    // GET AXIS DIRECTIONS
    vec3 up = normalize(cross(direction, vec3(0.0, 1.0, 0.0)));
    vec3 right = normalize(cross(up, direction));
    
    // GET FOCUS DISTANCE
    float focus_distance = length(center - cam_pos) * FOCAL_LENGTH;
    
    vec3 out_color = vec3(0.0, 0.0, 0.0);

    if (USE_DOF) {
        vec3 dof_color = vec3(0.0, 0.0, 0.0);
        for (int i = 0; i < NUM_SAMPLES; i++) {
            // GENERATE SAMPLE
            float randX = (2.0 * rand(uv.xy + vec2(cos(float(i)), sin(float(i))))) - 1.0;
            float randY = (2.0 * rand(uv.xy - vec2(sin(float(i)), cos(float(i))))) - 1.0;
            vec3 aperture_offset = vec3(randX, randY, 0.0) * APERTURE;

            // NEW ORIGIN / DIRECTION
            vec3 new_cam_pos = cam_pos + aperture_offset;
            vec3 focal_point = cam_pos + direction * FOCAL_LENGTH;
            vec3 new_direction = normalize(focal_point - new_cam_pos);

            // CALCULATE SAMPL<E
            vec3 sampleColor = ray_march_fractal(new_cam_pos, new_direction);

            // ACCUMULATE COLOR
            dof_color = dof_color + sampleColor;
        }
        // GET AVERAGE COLOR
        dof_color = dof_color / float(NUM_SAMPLES);
        out_color = dof_color;
    }
    else {
        out_color = ray_march_fractal(cam_pos, direction);
    }

    // OUTPUT COLOR
    color = vec4(out_color, 1.0);
}
