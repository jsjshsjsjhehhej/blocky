#pragma once

/**
 * Header precompilato per libblocky.dll (main.cpp, studio_main.cpp,
 * BlockyApi.cpp). Contiene solo roba pesante e stabile (libreria grafica,
 * ImGui, glm, STL comune) che non cambia mai tra una modifica e l'altra:
 * il compilatore la analizza una volta sola invece che a ogni file.
 *
 * Niente Windows/winsock qui apposta: main.cpp e gli altri gestiscono
 * quell'ordine di include da soli (winsock2.h prima di windows.h), e
 * forzarlo qui rischierebbe di romperlo diversamente in file diversi.
 */

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
