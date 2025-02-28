#include "Renderer.h"
#include "Logger.h"

Renderer::Renderer()
{
}


Renderer::~Renderer()
{
}

float Renderer::mix(const float & a, const float & b, const float & mix)
{
	return b * mix + a * (1 - mix);
}


Vec3f Renderer::trace(const Vec3f & rayorig, const Vec3f & raydir,SphScene scene, const int & depth)
{
	//if (raydir.length() != 1) std::cerr << "Error " << raydir << std::endl;
	float tnear = INFINITY;
	const Sphere* sphere = NULL;
	// find intersection of this ray with the sphere in the scene
	for (unsigned i = 0; i < scene.GetSize(); ++i) {
		float t0 = INFINITY, t1 = INFINITY;
		//if (scene.getSphere(i).intersect(rayorig, raydir, t0, t1)) {
		if(scene.DoesSphereIntersect(i,rayorig,raydir,t0,t1)){
			if (t0 < 0) t0 = t1;
			if (t0 < tnear) {
				tnear = t0;
				sphere = &scene.getSphere(i);
			}
		}
	}
	// if there's no intersection return black or background color
	if (!sphere) return Vec3f(0,1.74,2);
	Vec3f surfaceColor = 0; // color of the ray/surfaceof the object intersected by the ray
	Vec3f phit = rayorig + raydir * tnear; // point of intersection
	Vec3f test = sphere->getCenter();
	Vec3f nhit = phit - sphere->getCenter(); // normal at the intersection point
	nhit.normalize(); // normalize normal direction
					  // If the normal and the view direction are not opposite to each other
					  // reverse the normal direction. That also means we are inside the sphere so set
					  // the inside bool to true. Finally reverse the sign of IdotN which we want
					  // positive.
	float bias = 1e-4; // add some bias to the point from which we will be tracing
	bool inside = false;
	if (raydir.dot(nhit) > 0) nhit = -nhit, inside = true;
	if ((sphere->getTransparency() > 0 || sphere->getReflection() > 0) && depth < MAX_RAY_DEPTH) {
		float facingratio = -raydir.dot(nhit);
		// change the mix value to tweak the effect
		float fresneleffect = mix(pow(1 - facingratio, 3), 1, 0.1);
		// compute reflection direction (not need to normalize because all vectors
		// are already normalized)
		Vec3f refldir = raydir - nhit * 2 * raydir.dot(nhit);
		refldir.normalize();
		Vec3f reflection = trace(phit + nhit * bias, refldir, scene, depth + 1);
		Vec3f refraction = 0;
		// if the sphere is also transparent compute refraction ray (transmission)
		if (sphere->getTransparency()) {
			float ior = 1.1, eta = (inside) ? ior : 1 / ior; // are we inside or outside the surface?
			float cosi = -nhit.dot(raydir);
			float k = 1 - eta * eta * (1 - cosi * cosi);
			Vec3f refrdir = raydir * eta + nhit * (eta *  cosi - sqrt(k));
			refrdir.normalize();
			refraction = trace(phit - nhit * bias, refrdir, scene, depth + 1);
		}
		// the result is a mix of reflection and refraction (if the sphere is transparent)
		surfaceColor = (
			reflection * fresneleffect +
			refraction * (1 - fresneleffect) * sphere->getTransparency()) * sphere->getSurfaceColor();
	}
	else {
		// it's a diffuse object, no need to raytrace any further
		for (unsigned i = 0; i < scene.GetSize(); ++i) {
			if (scene.getSphere(i).getEmissionsColor().x > 0) {
				// this is a light
				Vec3f transmission = 1;
				Vec3f lightDirection = scene.getSphere(i).getCenter() - phit;
				lightDirection.normalize();
				for (unsigned j = 0; j < scene.GetSize(); ++j) {
					if (i != j) {
						float t0, t1;
						if (scene.getSphere(j).intersect(phit + nhit * bias, lightDirection, t0, t1)) {
							transmission = 0;
							break;
						}
					}
				}
				surfaceColor += sphere->getSurfaceColor() * transmission *
					std::max(float(0), nhit.dot(lightDirection)) * scene.getSphere(i).getEmissionsColor();
			}
		}
	}

	return surfaceColor + sphere->getEmissionsColor();
}

void Renderer::traceThread(const Vec3f &rayorig, const Vec3f &raydir, SphScene scene, const int &depth, int y, int width, int x){
	//trace(rayorig, raydir, scene, depth);
	pixel[y*width + x] = trace(rayorig, raydir, scene, depth);
}
void Renderer::render(SphScene scene, int iteration, const char * folderName)
{

#ifdef _DEBUG
	unsigned width = 640, height = 480;
	//unsigned width = 100, height = 100;
#else
#ifdef _4K
	unsigned width = 3940, height = 2160;
#else
	unsigned width = 1920, height = 1080;
#endif
#endif

	Vec3f *image = new Vec3f[width * height];
	pixel = image;
	float invWidth = 1 / float(width), invHeight = 1 / float(height);
	float fov = 80, aspectratio = width / float(height);
	float angle = tan(M_PI * 0.5 * fov / 180.);
	// Trace rays

	ThreadRend(width, height, angle, aspectratio, invWidth, invHeight, scene, pixel);

	// Save result to a PPM image (keep these flags if you compile under Windows)
	std::stringstream ss;

	ss << "./" << folderName << "/spheres" << workOutInt(iteration) << iteration << ".ppm";

	std::string tempString = ss.str();
	char* filename = (char*)tempString.c_str();
#ifdef _NONTHREAD
	ThreadFile(filename,width,height,image);
#else
	readThread = std::thread(&Renderer::ThreadFile, this, tempString, width, height, image);
#endif
	/*std::ofstream ofs(filename, std::ios::out | std::ios::binary);
	ofs << "P6\n" << width << " " << height << "\n255\n";
	for (unsigned i = 0; i < width * height; ++i) {
		ofs << (unsigned char)(std::min(float(1), image[i].x) * 255) <<
			(unsigned char)(std::min(float(1), image[i].y) * 255) <<
			(unsigned char)(std::min(float(1), image[i].z) * 255);
	}
	ofs.close();
	delete[] image;*/
}
void Renderer::ThreadFile(std::string fileString, unsigned width, unsigned height, Vec3f* image){
	char* filename2 = (char*)fileString.c_str();
	std::ofstream ofs(filename2, std::ios::out | std::ios::binary);
	ofs << "P6\n" << width << " " << height << "\n255\n";
	for (unsigned i = 0; i < width * height; ++i) {
		ofs << (unsigned char)(std::min(float(1), image[i].x) * 255) <<
			(unsigned char)(std::min(float(1), image[i].y) * 255) <<
			(unsigned char)(std::min(float(1), image[i].z) * 255);
	}
	ofs.close();
	delete[] image;
}
void Renderer::ThreadRend(unsigned width, unsigned height, float angle, float aspectratio, float invWidth, float invHeight, SphScene& scene, Vec3f* pixel){
	//run insted of loops to turn off threading
#ifdef _NONTHREAD
	ThreadSplitter(0, height, 0,width, width, scene, invWidth, invHeight, angle, aspectratio);

#else	
	const int HeightThreads = 2;
	const int widthThreads = 2;
	const int threadsUsed = HeightThreads *widthThreads;
	int hightPer = height/ HeightThreads, widthPer = width/ widthThreads;
	std::thread threadPool[threadsUsed];
	for (int i = 0; i < HeightThreads; ++i){
		for (int j = 0; j < widthThreads; ++j){
			threadPool[(i*widthThreads)+j] = std::thread(&Renderer::ThreadSplitter, this, i*hightPer, (i + 1)*hightPer, j*widthPer, (j + 1)*widthPer, width, scene, invWidth, invHeight, angle, aspectratio);
			
		}
	}
	for (int i = 0; i < (threadsUsed); ++i){
		threadPool[i].join();
	}
	//std::cout << "render Done" << "\n";
	Logger::output("render Done \n");
	JoinReadThread();
#endif
}
void Renderer::JoinReadThread(){
	try{
		readThread.join();
		//std::cout << "file save done" << "\n";
		Logger::output("file save done \n");
	}
	catch (std::exception) {

	}
}
void Renderer::ThreadSplitter(unsigned startHeight, unsigned height, unsigned startWidth, unsigned width, unsigned Totalwidth, SphScene& scene, float invWidth, float invHeight, float angle, float aspectratio){
	for (unsigned y = startHeight; y < height; ++y) {
		for (unsigned x = startWidth; x < width; ++x) {

			float xx = (2 * ((x + 0.5) * invWidth) - 1) * angle * aspectratio;
			float yy = (1 - 2 * ((y + 0.5) * invHeight)) * angle;
			Vec3f raydir(xx, yy, -1);
			raydir.normalize();
			traceThread(Vec3f(0), raydir, scene, 0, y, Totalwidth, x);
		}
	}
}
const char * Renderer::workOutInt(int iteration)
{
	const char* temp = (const char*)iteration;
	if (iteration < 10) {
		temp = "000" ;
	}
	else if (iteration < 100) {
		temp = "00" ;
	}
	else if(iteration < 1000){
		temp = "0" ;
	}
	return "";
}
