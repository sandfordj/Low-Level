#include "Renderer.h"


Renderer::Renderer()
{
	int ret = SCE_OK;

	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_FIBER);
	assert(ret == SCE_OK);

	

}


Renderer::~Renderer()
{
}

float Renderer::mix(const float &a, const float &b, const float &mix)
{
	return b * mix + a * (1 - mix);
}


Vec3f Renderer::trace(
	const Vec3f &rayorig,
	const Vec3f &raydir,
	SphScene scene,
	const int &depth)
{
	//if (raydir.length() != 1) std::cerr << "Error " << raydir << std::endl;
	float tnear = INFINITY;
	const Sphere* sphere = NULL;
	// find intersection of this ray with the sphere in the scene
	for (unsigned i = 0; i < scene.GetSize(); ++i) {
		float t0 = INFINITY, t1 = INFINITY;
		if (scene.DoesSphereIntersect(i, rayorig, raydir, t0, t1)) {
			if (t0 < 0) t0 = t1;
			if (t0 < tnear) {
				tnear = t0;
				sphere = scene.getSphereRef(i);
			}
		}
	}
	// if there's no intersection return black or background color
	if (!sphere) return Vec3f(2);
	Vec3f surfaceColor = 0; // color of the ray/surfaceof the object intersected by the ray
	Vec3f phit = rayorig + raydir * tnear; // point of intersection
	Vec3f nhit = phit - sphere->center; // normal at the intersection point
	nhit.normalize(); // normalize normal direction
	// If the normal and the view direction are not opposite to each other
	// reverse the normal direction. That also means we are inside the sphere so set
	// the inside bool to true. Finally reverse the sign of IdotN which we want
	// positive.
	float bias = 1e-4; // add some bias to the point from which we will be tracing
	bool inside = false;
	if (raydir.dot(nhit) > 0) nhit = -nhit, inside = true;
	if ((sphere->transparency > 0 || sphere->reflection > 0) && depth < MAX_RAY_DEPTH) {
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
		if (sphere->transparency) {
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
			refraction * (1 - fresneleffect) * sphere->transparency) * sphere->surfaceColor;
	}
	else {
		// it's a diffuse object, no need to raytrace any further
		for (unsigned i = 0; i < scene.GetSize(); ++i) {
			if (scene.getSphere(i).emissionColor.x > 0) {
				// this is a light
				Vec3f transmission = 1;
				Vec3f lightDirection = scene.getSphere(i).center - phit;
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
				surfaceColor += sphere->surfaceColor * transmission *
					std::max(float(0), nhit.dot(lightDirection)) * scene.getSphere(i).emissionColor;
			}
		}
	}

	return surfaceColor + sphere->emissionColor;
}


void Renderer::render(SphScene scene, int iteration, const char* folderName)
{

	//auto start = std::chrono::system_clock::now();

	// Initialize the WB_ONION memory allocator

	op[0] = 0; op[1] = 0; op[2] = 0;
	writeFH = 0;
	openParams = SCE_FIOS_OPENPARAMS_INITIALIZER;
	openParams.openFlags = SCE_FIOS_O_WRONLY | SCE_FIOS_O_CREAT | SCE_FIOS_O_TRUNC;


	int ret = onionAllocator.initialize(
		kOnionMemorySize, SCE_KERNEL_WB_ONION,
		SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_ALL);

	//if (ret != SCE_OK)
	//	return ret;
#ifdef _DEBUG
	unsigned width = 640, height = 480;
#else
	unsigned width = 1920, height = 1080;
#endif
	
	size_t totalSize = sizeof(Vec3f)* width * height;

	buffer = onionAllocator.allocate(totalSize, sce::Gnm::kAlignmentOfBufferInBytes);

	image = reinterpret_cast<Vec3f *>(buffer);
	pixel = image;

	float invWidth = 1 / float(width), invHeight = 1 / float(height);
	float fov = 30, aspectratio = width / float(height);
	float angle = tan(M_PI * 0.5 * fov / 180.);
	// Trace rays
	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x, ++pixel) {
			float xx = (2 * ((x + 0.5) * invWidth) - 1) * angle * aspectratio;
			float yy = (1 - 2 * ((y + 0.5) * invHeight)) * angle;
			Vec3f raydir(xx, yy, -1);
			raydir.normalize();
			*pixel = trace(Vec3f(0), raydir, scene, 0);
		}
	}
	// Save result to a PPM image (keep these flags if you compile under Windows)
	std::stringstream ss;
	ss << folderName <<"/spheres" << iteration << ".ppm";
	std::string tempString = ss.str();
	char* filename = (char*)tempString.c_str();

	std::ofstream ofs(filename, std::ios::out | std::ios::binary);
	ofs << "P6\n" << width << " " << height << "\n255\n";
	for (unsigned i = 0; i < width * height; ++i) {
		ofs << (unsigned char)(std::min(float(1), image[i].x) * 255) <<
			(unsigned char)(std::min(float(1), image[i].y) * 255) <<
			(unsigned char)(std::min(float(1), image[i].z) * 255);
	}
	ofs.close();

	unsigned char x, y, z;
	//output.clear();
	
	/*
	op[0] = sceFiosFHOpenSync(NULL, &writeFH, filename, &openParams);
	//assert(op[0] != SCE_FIOS_OP_INVALID);

	std::stringstream output;
	output << "P6\n" << width << " " << height << "\n255\n";
	SceFiosSize outputSize = (SceFiosSize)(strlen(output.str().c_str()) + 1);
	op[1] = sceFiosFHWriteSync
		(NULL, writeFH, output.str().c_str(), outputSize);
	//assert(op[1] != SCE_FIOS_OP_INVALID);
	result = sceFiosOpSyncWaitForIO(op[1]);
	assert(result != SCE_FIOS_OK);
	for (unsigned i = 0; i < width * height; ++i) {
		output.str("");
		output << (unsigned char)(std::min(float(1), image[i].x) * 255)
		<<
		(unsigned char)(std::min(float(1), image[i].y) * 255)
		<<
		(unsigned char)(std::min(float(1), image[i].z) * 255);

		outputSize = (SceFiosSize)(strlen(output.str().c_str()) + 1);
		op[1] = sceFiosFHWriteSync
			(NULL, writeFH, output.str().c_str(), outputSize);
		//assert(op[1] != SCE_FIOS_OP_INVALID);
		result = sceFiosOpSyncWaitForIO(op[1]);
		assert(result != SCE_FIOS_OK);

	};
	


	
	op[2] = sceFiosFHClose(NULL, writeFH);


	result = sceFiosOpSyncWait(op[0]);
	assert(result != SCE_FIOS_OK);
	result = sceFiosOpSyncWait(op[2]);
	assert(result != SCE_FIOS_OK);*/
	//delete[] image;
}