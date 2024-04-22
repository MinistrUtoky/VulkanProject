#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

void Camera::update() {
	glm::mat4 cameraRotationMatrix = getRotationMatrix();
	position += glm::vec3(cameraRotationMatrix * glm::vec4(0.5f * velocity, 0.f));
	if (position.x > limitsXYZ.x) position.x = limitsXYZ.x;
	else if (position.x < -limitsXYZ.x) position.x = -limitsXYZ.x;
	if (position.y > limitsXYZ.y) position.y = limitsXYZ.y;
	else if (position.y < -limitsXYZ.y) position.y = -limitsXYZ.y;
	if (position.z > limitsXYZ.z) position.z = limitsXYZ.z;
	else if (position.z < -limitsXYZ.z) position.z = -limitsXYZ.z;
}

void Camera::processSDLEvent(SDL_Event& sdlEvent) {
	if (sdlEvent.type == SDL_KEYDOWN) {
		if (sdlEvent.key.keysym.sym == SDLK_w)
			velocity.z = -1;
		if (sdlEvent.key.keysym.sym == SDLK_s)
			velocity.z = 1;
		if (sdlEvent.key.keysym.sym == SDLK_a)
			velocity.x = -1;
		if (sdlEvent.key.keysym.sym == SDLK_d)
			velocity.x = 1;
	}
	if (sdlEvent.type == SDL_KEYUP) {
		if (sdlEvent.key.keysym.sym == SDLK_w)
			velocity.z = 0;		
		if (sdlEvent.key.keysym.sym == SDLK_s)
			velocity.z = 0;
		if (sdlEvent.key.keysym.sym == SDLK_a)
			velocity.x = 0;
		if (sdlEvent.key.keysym.sym == SDLK_d)
			velocity.x = 0;
	}
	if (sdlEvent.type == SDL_MOUSEMOTION) {
		yaw += (float)sdlEvent.motion.xrel / 500.f;
		pitch -= (float)sdlEvent.motion.yrel / 500.f;
	}
}

glm::mat4 Camera::getViewMatrix() {
	glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
	glm::mat4 cameraRotation = getRotationMatrix();
	return glm::inverse(cameraTranslation * cameraRotation);
}
glm::mat4 Camera::getRotationMatrix() {
	glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3(1.f, 0.f, 0.f));
	glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3(0.f, -1.f, 0.f));
	return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}