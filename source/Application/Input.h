#pragma once

#include "KeyCodes.h"
#include <cstring> // For memcpy

class Window;

class Input
{
public:
	friend class Window;

	static inline bool isKeyDown(Key::KeyCode key);
	static inline bool isKeyPressed(Key::KeyCode key);
	static inline bool isKeyReleased(Key::KeyCode key);

private:
	static inline void update();
	static inline void setKeyDown(Key::KeyCode key);
	static inline void setKeyUp(Key::KeyCode key);

	static inline bool keys[Key::NUM_KEYS]{};
	static inline bool prevKeys[Key::NUM_KEYS]{};
};

inline bool Input::isKeyDown(Key::KeyCode key)		{ return Input::keys[key]; }
inline bool Input::isKeyPressed(Key::KeyCode key)	{ return Input::keys[key] && !Input::prevKeys[key]; }
inline bool Input::isKeyReleased(Key::KeyCode key)	{ return !Input::keys[key] && Input::prevKeys[key]; }

inline void Input::setKeyDown(Key::KeyCode key)		{ if (key >= Key::NUM_KEYS) return; keys[key] = true; }
inline void Input::setKeyUp(Key::KeyCode key)		{ if (key >= Key::NUM_KEYS) return; keys[key] = false; }

inline void Input::update()
{
	memcpy(Input::prevKeys, Input::keys, size_t(Key::NUM_KEYS));
}
