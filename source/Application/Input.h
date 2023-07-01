#pragma once
#include "KeyCodes.h"

#include "glm/glm.hpp"

#include <cstring> // For memcpy


class Window;

enum MouseMode : int
{
	Normal		= GLFW_CURSOR_NORMAL,
	Locked		= GLFW_CURSOR_DISABLED,
};

class Input
{
public:
	friend class Window;


	static inline bool isKeyDown(Key::KeyCode key);
	static inline bool isKeyPressed(Key::KeyCode key);
	static inline bool isKeyReleased(Key::KeyCode key);

	static inline glm::vec2 getMousePos();
	static inline glm::vec2 getMouseDelta();
	static inline void setMouseMode(MouseMode mode);

private:
	// NOTE: Fixes mouse inputs temporarily, 'should' be window specific
	static inline GLFWwindow* pWindow;

	static inline void update();
	static inline void setKeyDown(Key::KeyCode key);
	static inline void setKeyUp(Key::KeyCode key);

	static inline bool keys[Key::NUM_KEYS]{};
	static inline bool prevKeys[Key::NUM_KEYS]{};

	static inline glm::vec2 mousePos{};
	static inline glm::vec2 prevMousePos{};

};

inline bool Input::isKeyDown(Key::KeyCode key)		{ return Input::keys[key]; }
inline bool Input::isKeyPressed(Key::KeyCode key)	{ return Input::keys[key] && !Input::prevKeys[key]; }
inline bool Input::isKeyReleased(Key::KeyCode key)	{ return !Input::keys[key] && Input::prevKeys[key]; }

inline void Input::setKeyDown(Key::KeyCode key)		{ if (key >= Key::NUM_KEYS) return; keys[key] = true; }
inline void Input::setKeyUp(Key::KeyCode key)		{ if (key >= Key::NUM_KEYS) return; keys[key] = false; }

inline glm::vec2 Input::getMousePos()				{ return Input::mousePos; }
inline glm::vec2 Input::getMouseDelta()				{ return Input::mousePos - Input::prevMousePos; }
inline void Input::setMouseMode(MouseMode mode)		{ glfwSetInputMode(pWindow, GLFW_CURSOR, mode); }

inline void Input::update()
{
	// Keyboard inputs
	memcpy(Input::prevKeys, Input::keys, size_t(Key::NUM_KEYS));
	
	// Mouse position
	double xPos, yPos;
	glfwGetCursorPos(pWindow, &xPos, &yPos);
	Input::prevMousePos = Input::mousePos;

	Input::mousePos.x = (float)xPos;
	Input::mousePos.y = (float)yPos;
}

