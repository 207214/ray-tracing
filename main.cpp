# include <iostream>
# include <vector>
# include <string>
# include <cstdint>
# include <unordered_map>
# include "Bitmap.h"

# include <memory>
# include <cfloat>
# include <cmath>

# include <chrono> // время рендеринга

// Трассировщик лучей
// Весна 2020
// Файлы Bitmap, а также обработка аргументов командной строки
// в начале функции main() предоставлены как шаблон для задания

const uint32_t RED  = 0x000000FF;
const uint32_t GREEN = 0x0000FF00;
const uint32_t BLUE  = 0x00FF0000;

const uint32_t WELL = 0x00C71585;
# define EPS 0.000001f

unsigned max_depth;
unsigned width;
unsigned height;

uint32_t background_color;

struct vec {
    float x;
    float y;
    float z;
    vec(): x(0), y(0), z(0) {}
    vec(float a, float b, float c): x(a), y(b), z(c) {}
    vec operator+(const vec &op) const { return vec(x+op.x, y+op.y, z+op.z); }
    vec operator-(const vec &op) const { return vec(x-op.x, y-op.y, z-op.z); }
    vec operator*(const vec &op) const { return vec(x*op.x, y*op.y, z*op.z); }
    vec operator*(const float value) const { return vec(x*value, y*value, z*value); }
    
    vec operator-() const { return vec(-x,-y,-z); }

    vec operator=(const vec &op) { x = op.x; y = op.y; z = op.z; return *this; }
    
    vec operator+=(const vec &op) { x += op.x; y += op.y; z += op.z; return *this; }
    vec operator-=(const vec &op) { x -= op.x; y -= op.y; z -= op.z; return *this; }
    
    vec operator*=(const float value) { x *= value; y *= value; z *= value; return *this; }
        
};

vec normalize(const vec &op) {
    float dot2 = op.x*op.x + op.y*op.y + op.z*op.z;
    if (!dot2) return op;
    float contr = 1/sqrtf(dot2);
    return vec(op.x*contr, op.y*contr, op.z*contr);
}

float dot(const vec &op1, const vec &op2) {
    return op1.x*op2.x + op1.y*op2.y + op1.z*op2.z;
}

vec cross(const vec &op1, const vec &op2) {
    return vec(op1.y*op2.z - op1.z*op2.y, -op1.x*op2.z + op1.z*op2.x, op1.x*op2.y - op1.y*op2.x);
}

class Object {
public:

    int material; // 0 - reflection, 1 - diffuse, 2 - reflection&refr
    float ior; // показатель преломления == "c/v"

    float ka; // когда нет света
    float ks; // вес бликовой компоненты
    float kd; // вес диффузной компоненты
    float p; // коэффициент гладкости
    uint32_t native_color; // " ... it is that essential color that the object reveals under pure white light ... "

    Object(): material(1), ior(1.333),  ka(0.4), ks(1), kd(0.5), p(5), native_color(GREEN) {}

    virtual ~Object() {}
    virtual void getNormal(const vec &, vec &) const = 0;
    virtual bool ray_intersect(const vec &, const vec &, float &) const = 0;
};

// 2D, стороны параллельны осям координат
class AABB : public Object { 

    float min_x, min_y, min_z, max_x, max_y, max_z;

    float far;

    int flag; // 0 - const  z, 1 - const y, 2 - const x;

public:

    AABB(float x1, float y1, float z1, float x2, float y2, float z2): min_x(x1), min_y(-y2), min_z(z1), max_x(x2), max_y(-y1), max_z(z2) {
        native_color = 0x00C71585;
        if (min_z == max_z) {
            far = min_z;
            flag = 0;
        }
        else if (min_y == max_y) {
            far = min_y;
            flag = 1;
        }
        else {
            far = min_x;
            flag = 2;
        }
    }

    bool ray_intersect(const vec &begin, const vec &dir, float &param_inter) const {
        vec straight(0,0,0);
        if (flag == 0) straight.z = far;
        else if (flag == 1) straight.y = far;
        else straight.x = far;

        float cos = dot(normalize(dir-begin), normalize(straight));
        float t = sqrtf(dot(straight, straight))/cos;

        vec Dir = dir;
        Dir *= t;

        if (  ( (flag == 0) && ( (Dir.x >= min_x)&&(Dir.x <= max_x)&&(Dir.y >= min_y)&&(Dir.y <= max_y) ) ) 
           || ( (flag == 1) && ( (Dir.x >= min_x)&&(Dir.x <= max_x)&&(Dir.z >= min_z)&&(Dir.z <= max_z) ) )
           || ( (flag == 2) && ( (Dir.y >= min_y)&&(Dir.y <= max_y)&&(Dir.z >= min_z)&&(Dir.z <= max_z) ) ) )
        {
            param_inter = t;
            return true;
        }
        return false;
    }
    
    void getNormal(const vec &l, vec &norm) const {
        if (flag == 0) {
            if (far < 0)
                norm = vec(0,0,-1);
            else norm = vec(0,0,1);
        }
        else if (flag == 1) {
            if (far < 0)
                norm = vec(0,1,0);
            else norm = vec(0,-1,0);
        }
        else {
            if (far < 0)
                norm = vec(1,0,0);
            else norm = vec(-1,0,0);
        }
    }
};

// плоскость
class Plane : public Object {
    vec point_1;
    vec point_2;
    vec point_3;

    vec diff_1;
    vec diff_2;    // их  векторное произведение дает нормаль к плоскости

public:    

    Plane(float coordConst = 10, int flag = 0) {
        if (!flag) {
            point_1 = vec(-1, coordConst, -1);
            point_2 = vec(-1, coordConst, -2);
            point_3 = vec(0, coordConst, -2);
        }
        else if (flag == 1) {
            point_1 = vec(-1, -1, coordConst);
            point_2 = vec(-1, -2, coordConst);
            point_3 = vec(0, -2, coordConst);
        }
        else {
            point_1 = vec(coordConst, -1, -1);
            point_2 = vec(coordConst, -1, -2);
            point_3 = vec(coordConst, 0, -2);
        }
        diff_1 = point_2 - point_1;
        diff_2 = point_3 - point_1;
    }
    
    void getNormal(const vec &l, vec &norm) const {     
        norm = cross(diff_1,diff_2);
    }

    vec getPoint_1() const { return point_1; } 
    vec getPoint_2() const { return point_2; }
    vec getPoint_3() const { return point_3; }
       
 // уравнение плоскости: dot ( p - point_1, cross(point_2 - point_1, point_3 - point_1) ) = 0   
    bool ray_intersect(const vec &begin, const vec &dir, float &param_inter) const {
        vec v1 = cross(diff_1, diff_2);
        float dirdot_v1 = dot(dir, v1);
        if (dirdot_v1)
            param_inter = dot(point_1, v1)/(float)(dirdot_v1);
//        std::cout << param_inter;
        if ((param_inter > 0) && (param_inter < 500))
            return true;
        return false;
    }
};
/*
class Cyl : public Object {

    vec a;
    vec b;
    float rad;
public:
    Cyl(const vec &p1, const vec &p2, const float &radius): a(p1), b(p2), rad(radius) {}
    Cyl() { rad = 1; a = vec(); b = vec(0,0,1); }
    bool ray_intersect(const vec &begin, const vec &dir, float &param_inter) const {
         
    }
};
*/

class Round : public Object {
    vec c;
    float rad;

public:
    Round(const vec &centre, const float &radius): c(centre), rad(radius) {}
    Round() { rad = 1; c = vec(); }
    bool ray_intersect(const vec &begin, const vec &dir, float &param_inter) const {
    
        vec l = begin - c;
        
    }
    
    void getNormal(const vec &l, vec &norm) const { norm = normalize(l - c); }
};

class Sphere : public Object {

    vec c;
    float rad;

public:

    Sphere(const vec &centre, const float &radius): c(centre), rad(radius) { /*c.y *= -1;*/ } // ?!?!some problem with raster->world or smth like that

    Sphere() { rad = 1, c = vec(); }

    bool ray_intersect(const vec &begin, const vec &dir, float &param_inter) const {
        
        vec l = begin - c;
        float a = dot(dir, dir);
        float b = 2 * dot(dir, l);
        float c = dot(l,l) - rad*rad;
        float t0 = 0;
        float t1 = 0;
        
        float D = b*b - 4*a*c;
        if (D < 0) return false;
        if (!D) {
            t0 = t1 = b/a * (-0.5);
//            param_inter = t1;//
            return true;
        }
        t0 = (-b + sqrtf(D))/(2*a);
        t1 = (-b - sqrtf(D))/(2*a);
        if (t1 < 0) t1 = t0; 
        param_inter = t1;
        return (param_inter >= 0);
        
       // http://viclw17.github.io/2018/07/16/raytracing-ray-sphere-intersection/
    }
    
    void getNormal(const vec &l, vec &norm) const { norm = normalize(l - c); }
};


class Eyes {

    int angle_y;

    vec watching_from;

public:

    int get_angle() const { return angle_y; }

    vec get_position() const { return watching_from; }

    Eyes(): angle_y(90), watching_from(vec()) {}

    void close(std::vector<uint32_t> &image) {
        for (auto& pixel : image)
            pixel = 0;
    }

};

struct Light {

    vec position;

    vec color;

    Light(const vec &p = vec(0,0,5), vec c = vec(1,1,1)): position(p), color(c) {}
};


inline
vec colorReverseTovec(uint32_t color, int i = 0) {
    return vec((color & 0x000000FF)/255.0f, ((color & 0x0000FF00)>>8)/255.0f, ((color & 0x00FF0000)>>16)/255.0f);
}


inline
uint32_t colorReverseToint(vec color, int i = 0) {
    uint32_t r =   (color.x * 255);
    uint32_t g =   (color.y * 255);
    uint32_t b =   (color.z * 255);
    return r | (g << 8) | (b << 16);
}


bool meetsObject(const vec &dir, const vec &camera, 
                  const std::vector<Object*> &objects, float &closest_param, Object **hitObject) {
    
    *hitObject = nullptr;
                  
    for (unsigned i = 0; i < objects.size(); ++i) {
        float tmp_param = FLT_MAX;
        bool inter = objects[i]->ray_intersect(camera, dir, tmp_param);
        if ( (inter) && (closest_param > tmp_param) && (tmp_param > 0) ) {
            closest_param = tmp_param;
            *hitObject = objects[i];
        }
        
    }
    return (*hitObject != nullptr);
}

vec ambient(Object *hitObject) {
    vec vecobj = colorReverseTovec(hitObject->native_color);
    vec vecamb(0.05,0.05,0.05);
    vec vecres(vecobj.x * vecamb.x, vecobj.y * vecamb.y, vecobj.z * vecamb.z);//
    vec r = vecres * hitObject->ka;
    return r;
}


float fresnel(const vec &dir, const vec &norm, const float ior)
{
    float cos_fall = fabsf(dot(norm, dir));
    float n1 = 1;
    float n2 = ior;

    float sin_through = n2 / n1 * sqrtf(std::max(0.0f,(float) (1 - cos_fall * cos_fall))); // закон Снеллиуса
    float cos_through = sqrtf(std::max(0.0f, (float) (1.0 - sin_through * sin_through))); // уже домножили на ratio, "синус" может быть >1

    float Rs = (n1 * cos_fall - n2 * cos_through) / (n1 * cos_fall + n2 * cos_fall);
    float Rp = (n1 * cos_through - n2 * cos_fall) / (n1 * cos_through + n2 * cos_fall);
    return  (Rs * Rs + Rp * Rp) / 2.0f;
    
    // https://en.wikipedia.org/wiki/Fresnel_equations
}


/*	закон отражения и закон преломления в векторной форме

 https://docplayer.ru/35463163-Zakon-prelomleniya-v-vektornoy-forme.html

*/ 
vec reflect(const vec &dir, const vec &N) {
    return dir - N * (2 * dot(dir, N));
}

vec refract(const vec &dir, const vec &N, const float &ior1, const float &ior2) {
    vec Dir = normalize(dir) * ior1;
    float a = dot(Dir, N);
    float b = -1 + sqrtf(1 + ((ior2*ior2 - ior1*ior1)/(a*a)));
    return N*(a*b);
    
}
/*
    закон отражения и закон преломления в векторной форме
*/


// главная функция, возвращающая цвет очередного пикселя
uint32_t buildPixelColor(const std::vector<Object*> &objects, const std::vector<Light*> &lights, 
                         const vec &dir, const vec &camera, unsigned depth) {
                                 
    if (depth > max_depth)
        return background_color; 

    float closest_param = FLT_MAX;
    Object *hitObject = nullptr;

    bool boom = meetsObject(dir, camera, objects,closest_param, &hitObject); // для луча из глаза
    if (!boom)
        return background_color;
  
    vec inter = camera + dir * closest_param;
    vec N; 
    hitObject->getNormal(inter,N);
    
    vec vechitColor =  ambient(hitObject);//фоновое освещение
     
    vec inter_modified = inter + N * ((( dot(dir, N) < 0)) * EPS) ;
    if (hitObject->material == 0) {
        vec reflectedLight = normalize(reflect(dir, N));
        return colorReverseToint(vechitColor) | buildPixelColor(objects, lights, reflectedLight, inter_modified, depth+1);
    }
    if (hitObject->material == 2) {
        vec reflectedLight = normalize(reflect(dir, N));
        vec refractedLight = normalize(refract(dir, N, 1, hitObject->ior));
        float k_fresnel = fresnel(dir, N, hitObject->ior);
        vec comp2 = colorReverseTovec(buildPixelColor(objects, lights, refractedLight, inter_modified, depth+1));
        vec comp1 = colorReverseTovec(buildPixelColor(objects, lights, reflectedLight, inter_modified, depth+1));
        comp1 *= k_fresnel;
        comp2 *= (1-k_fresnel);
        return colorReverseToint(vechitColor) | colorReverseToint(comp1+comp2);
 
    }

    vec vechitNativeColor = colorReverseTovec(hitObject->native_color);
      
    vec diffuse;
    vec glare;


    for (unsigned i = 0; i < lights.size(); ++i) {
    
        vec toLight = lights[i]->position - inter;
        float toLightDist = dot(toLight, toLight);
        toLight = normalize(toLight);
        
        Object *hitObject2 = nullptr;
        float closest_param2 = FLT_MAX;
               
        boom = meetsObject(toLight, inter_modified, objects, closest_param2, &hitObject2); // для луча с видимой точки до источника, проверка на преграды
        
        vec reflectedLight = normalize(reflect(-toLight, N));
        
        if ((!boom) || (closest_param2 >= sqrtf(toLightDist))) {	//модель Фонга
            diffuse = diffuse +  lights[i]->color * ( std::max(0.0f, dot(toLight, N)));
            glare = glare + lights[i]->color * ( powf(std::max(0.0f, dot(normalize(camera - inter), reflectedLight)), hitObject->p));
        }
        
    }
    vechitColor += vec(diffuse.x*vechitNativeColor.x, diffuse.y*vechitNativeColor.y, diffuse.z*vechitNativeColor.z)*hitObject->kd+glare*hitObject->ks;
    if (vechitColor.x > 1) { /*std::cout << "XXX!\n" << std::endl;*/ vechitColor.x = 1; }
    if (vechitColor.y > 1) { /*std::cout << "YYY!\n" << std::endl;*/ vechitColor.y = 1; }
    if (vechitColor.z > 1) { /*std::cout << "ZZZ!\n" << std::endl;*/ vechitColor.z = 1; }
       
    return colorReverseToint(vechitColor);
}


void raster_to_worldspace(uint32_t raster_x, uint32_t raster_y, int angle,
                          float &world_x, float &world_y, float &world_z) {
                          
    float pixel_ndc_x = (raster_x + 0.5)/width;
    float pixel_ndc_y = (raster_y + 0.5)/height; // нормированные координаты пикселей, начало координат - левый верхний угол
    
    float screen_x = (2*pixel_ndc_x - 1)*(width/(float)height);
    float screen_y = 1 - 2*pixel_ndc_y; // координаты в пространстве экрана ( то есть квадрата с началом координат в его центре)
    
    world_x = screen_x * tan( ( (angle * M_PI) / 180) * 0.5);
    world_y = screen_y * tan( ( (angle * M_PI) / 180) * 0.5);
    world_z = -1.0; // смотрим в отрицательную сторону оси z
    
    // https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-generating-camera-rays/generating-camera-rays
}

void open_eyes(const std::vector<Object*> &objects, 
               const std::vector<Light*> &lights,
               Eyes viewer, uint32_t back_color, std::vector<uint32_t> &image) {
    
    vec camera(0, 0, 0);
    
    int k = 0;
    int angle = viewer.get_angle();
    
    for (unsigned j = 0; j < height; ++j) {
        for (unsigned i = 0; i < width; ++i) {

            float x = 0.0;
            float y = 0.0;
            float z = 0.0;            
            raster_to_worldspace(i, j, angle, x, y, z);                                  
            vec dir = normalize(vec(x,y,z));
            
            
            image[k++] = buildPixelColor(objects, lights, dir, camera, 0);
            
        }
    }
}

/*
// имитируется множество Кантора 
void cantor(char *str, int step, float x, int nonstop = 0) {
    if (!nonstop) {
        free(str);
        return;
    }
    int k = 0;
    int len = strlen(str);
    char *newstr = (char*)calloc(len*3+1, sizeof(char));
    for (int i = 0; i < len; ++i ) {
        if (str[i] == 'a') {

            steps[step] = new AABB(x, -0.5+(1.5*(i/(float)len))-HM, -1.3, x+0.1, -0.5+(1.5*((i+1)/(float)len))-HM, -1.3);
            steps[step]->material = 1;
            steps[step]->native_color = BLUE;//0xFFFFFF;
            objects.push_back	(steps[step++]);
            
            steps[step] = new AABB(x, -0.5+(1.5*(i/(float)len))-HM, -1.9, x, -0.5+(1.5*((i+1)/(float)len))-HM, -1.3);
            steps[step]->material = 1;
//            if (i == 0) steps[step]->material = 0;
            steps[step]->native_color = RED; //0xFFFFFF;
            objects.push_back	(steps[step++]);
    
            steps[step] = new AABB(x+0.1, -0.5+(1.5*(i/(float)len))-HM, -1.9, x+0.1, -0.5+(1.5*((i+1)/(float)len))-HM, -1.3);
            steps[step]->material = 1;
            steps[step]->native_color = BLUE;
            objects.push_back	(steps[step++]);

            steps[step] = new AABB(x, -0.5+(1.5*(i/(float)len))-HM, -1.9, x+0.1, -0.5+(1.5*(i/(float)len))-HM, -1.3);
            steps[step]->material = 1;
            steps[step]->native_color = BLUE;
            objects.push_back	(steps[step++]);

            steps[step] = new AABB(x, -0.5+(1.5*((i+1)/(float)len))-HM, -1.9, x+0.1, -0.5+(1.5*((i+1)/(float)len))-HM, -1.3);
            steps[step]->material = 1;
            steps[step]->native_color = BLUE; //0xFFFFFF;
            objects.push_back	(steps[step++]);

            steps[step] = new AABB(x, -0.5+(1.5*(i/(float)len))-HM, -1.9, x+0.1, -0.5+(1.5*((i+1)/(float)len))-HM, -1.9);
            steps[step]->material = 1;
            steps[step]->native_color = BLUE; //0xFFFFFF;
            objects.push_back	(steps[step++]);
            
            steps = (AABB**)realloc(steps, sizeof(AABB*)*step*2);

            newstr[k++] = 'a';
            newstr[k++] = 'b';
            newstr[k++] = 'a';
        }
        else {
            newstr[k++] = 'b';
            newstr[k++] = 'b';
            newstr[k++] = 'b';
        }
    }
    newstr[k] = '\0';
    free(str);
    x -= 0.2;
    std::cout << "\n X == " << x << "\n";
    cantor(newstr, step, x, --nonstop);
}
*/


int main(int argc, const char** argv) {
    std::unordered_map<std::string, std::string> cmdLineParams;
    for (int i=0; i<argc; i++) {
            std::string key(argv[i]);

        if (key.size() > 0 && key[0]=='-') {
            if (i != argc-1) { // not last argument
              cmdLineParams[key] = argv[i+1];
              i++;
            }
            else
                cmdLineParams[key] = "";
        }
    }

    std::string outFilePath = "zout.bmp";
    if (cmdLineParams.find("-out") != cmdLineParams.end())
        outFilePath = cmdLineParams["-out"];

    int sceneId = 0;
        sceneId = atoi(cmdLineParams["-scene"].c_str());


    Eyes eye;
    max_depth = 3;
    width = 2048/0.5;
    height = 2048/0.5;

    background_color = 0;// 0x4169E1;

    if (sceneId == 1)
        background_color = 0;
    else return 0;
    

    std::vector<uint32_t> image(height*width);

    for (auto& pixel : image)
        pixel = background_color;

    std::vector<Object*> objects;
    std::vector<Light*> lights;

    Sphere *sph_1 = new Sphere(vec(-0-0.6, 0.5, -1.6), 0.4);
    sph_1->native_color = 0;//GREEN|RED|BLUE;
    sph_1->ka = 0.5;
    sph_1->ks = 0.6;
//    sph_1->material = 0;//2;
    sph_1->ior = 1.6;
    Sphere *sph_2 = new Sphere(vec(0.4, 0, -1.2), 0.2);
    sph_2->native_color = BLUE;//RED|GREEN;//0;//BLUE|RED|GREEN;//BLUE;
//    sph_2->material = 0;
    sph_2->ks = 0.7;
    Sphere *sph_3 = new Sphere(vec(0.3,0,-1.6), 0.04);
    sph_3->native_color = RED;
    
    Sphere *sph_4 = new Sphere(vec(0.27, 0, -0.4), 0.1);
    sph_4->material = 0;
    sph_4->native_color = GREEN;
    sph_4->ior = 1.6;
    
    Sphere *sph_5 = new Sphere(vec(-0.3,0.3,-0.5), 0.05);    
    sph_5->native_color = 0;
    sph_5->material = 0;

    AABB *left_wall = new AABB(-1.0,-1,-2,-1.0,1,1);
    left_wall->native_color = BLUE;//;RED|BLUE|GREEN;//BLUE; //GREEN;
    left_wall->material = 0;
    
    AABB *right_wall = new AABB(1,-1,-2,1,2,1);
    right_wall->native_color = BLUE|GREEN|RED;//0xD;
  //  right_wall->material = 0;
    
    Plane *ground = new Plane(1, 0);    
    ground->native_color = 0x202020;
//    ground->material = 0;
    AABB *frontwall = new AABB(-2,-2.0, -2,2,2,-2);
    frontwall->material = 0;
     
    AABB *backwall = new AABB(-2, -1.5, 1, 2, 4, 1);
    backwall->native_color = BLUE|GREEN|RED;

    AABB *skirtingboard_1 = new AABB(-0.99,-1,-1.99,0.99,-0.96,-1.99);
    skirtingboard_1->native_color = 0xFFFFFF;

    AABB *skirtingboard_2 = new AABB(-0.99,-1,-1.99,-0.99,-0.96,1.01);
    skirtingboard_2->native_color = 0xFFFFFF;
    
    AABB *skirtingboard_3 = new AABB(0.99,-1,-1.99,0.99,-0.96,1.01);
    skirtingboard_3->native_color = 0xFFFFFF;


    objects.push_back	(sph_1);    
    objects.push_back	(sph_2);
//    objects.push_back	(sph_3);
    objects.push_back	(sph_4);
//    objects.push_back	(sph_5);
    objects.push_back	(left_wall);
    objects.push_back	(right_wall);
    objects.push_back	(ground);
    objects.push_back	(frontwall);
    objects.push_back	(backwall);
  //  objects.push_back	(skirtingboard_1);
//    objects.push_back	(skirtingboard_2);
  //  objects.push_back	(skirtingboard_3);   

  
    Light *light_1 = new Light(vec(0.5, -0.8, -1.0));
    lights.push_back(light_1);
    light_1->color = colorReverseTovec(RED);
    
    Light *light_2 = new Light(vec(-0.8,0,-1.0));
    lights.push_back(light_2);
    light_2->color = colorReverseTovec(0xfff917);
    
    Light *light_3 = new Light(vec(-0.5,0.8,-1.2));
    lights.push_back(light_3);
    light_3->color = colorReverseTovec(RED|GREEN);
    
    
    auto start = std::chrono::high_resolution_clock::now();


    open_eyes(objects, lights, eye, background_color, image);



    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop-start);
    // https://www.geeksforgeeks.org/measure-execution-time-function-cpp/

    std::cout << "Время рендеринга == " << duration.count()/1000000.0f << " секунд.\n";


    SaveBMP(outFilePath.c_str(), image.data(), width, height);
    std::cout << "end." << std::endl;    
    objects.clear();
    lights.clear();
    return 0;
}
