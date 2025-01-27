/*
  DrawMapOpenGL - a demo program for libosmscout
  Copyright (C) 2018  Alexis Metge

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <DrawMap.h>

#include <fstream>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <iomanip>

#include <osmscout/Database.h>
#include <osmscout/util/CmdLineParsing.h>
#include <osmscout/util/Logger.h>

#include <osmscoutmap/MapService.h>

#include <osmscoutmapopengl/MapPainterOpenGL.h>

#include <GLFW/glfw3.h>

void GLAPIENTRY MessageCallback([[maybe_unused]] GLenum source,
                                GLenum type,
                                [[maybe_unused]] GLuint id,
                                GLenum severity,
                                [[maybe_unused]] GLsizei length,
                                const GLchar* message,
                                [[maybe_unused]] const void* userParam)
{
  std::stringstream msgType;

  switch (type) {
    case GL_DEBUG_TYPE_ERROR:
      msgType << "ERROR";
      break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      msgType << "DEPRECATED_BEHAVIOR";
      break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
      msgType << "UNDEFINED_BEHAVIOR";
      break;
    case GL_DEBUG_TYPE_PORTABILITY:
      msgType << "PORTABILITY";
      break;
    case GL_DEBUG_TYPE_PERFORMANCE:
      msgType << "PERFORMANCE";
      break;
    case GL_DEBUG_TYPE_OTHER:
      msgType << "OTHER";
      break;
    default:
      msgType << "type " << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(type);
  }

  msgType << " (";
  switch (severity) {
    case GL_DEBUG_SEVERITY_NOTIFICATION:
      msgType << "notification";
      break;
    case GL_DEBUG_SEVERITY_HIGH:
      msgType << "high";
      break;
    case GL_DEBUG_SEVERITY_MEDIUM:
      msgType << "medium";
      break;
    case GL_DEBUG_SEVERITY_LOW:
      msgType << "low";
      break;
    default:
      msgType << "severity " << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(severity);
  }
  msgType << ")";

  osmscout::log.Debug() << "GL " << msgType.str() << ": " << message ;
}


/*
  Example for the nordrhein-westfalen.osm (to be executed in the Demos top
  level directory):

  DrawMapOpenGL ../maps/nordrhein-westfalen ../stylesheets/standard.oss 1024 800 51.51241 7.46525 70000 test.ppm
 */

int main(int argc, char* argv[]) {
  DrawMapDemo drawDemo("DrawMapOpenGL", argc, argv, 96.0, ARG_WS_CONSOLE, DEFAULT_FONT_FILE);

  std::string shaderPath=SHADER_INSTALL_DIR;
  drawDemo.argParser.AddOption(osmscout::CmdLineStringOption([&shaderPath](const std::string& value) {
                                 shaderPath=value;
                               }),
                               "shaders",
                               "Path to shaders (default: " + shaderPath + ")",
                               false);

  if (!drawDemo.OpenDatabase()){
    return 2;
  }

  drawDemo.LoadData();
  Arguments args = drawDemo.GetArguments();

  // Create the offscreen renderer
  glfwSetErrorCallback([](int, const char *err_str) {
    std::cerr << "GLFW Error: " << err_str << std::endl;
  });
  if (!glfwInit()) {
    return 1;
  }
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_VISIBLE, false);
  GLFWwindow* offscreen_context = glfwCreateWindow(args.width, args.height, "", NULL, NULL);
  if (!offscreen_context) {
    std::cerr << "Failed to create offscreen context." << std::endl;
    return 1;
  }
  glfwMakeContextCurrent(offscreen_context);

  osmscout::MapPainterOpenGL* painter = new osmscout::MapPainterOpenGL(args.width, args.height, args.dpi, args.fontName, shaderPath, drawDemo.drawParameter);

  if (!painter->IsInitialized()) {
    delete painter;
    return 1;
  }

  painter->SetCenter(drawDemo.projection.GetCenter());
  painter->SetMagnification(drawDemo.projection.GetMagnification());

  painter->ProcessData(drawDemo.data, drawDemo.projection, drawDemo.styleConfig);
  painter->SwapData();

  painter->DrawMap();

  if (args.debug) {
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);
  }

  // Save to file
  unsigned char* image = new unsigned char[3 * args.width * args.height];
  glReadPixels(0, 0, args.width, args.height, GL_RGB, GL_UNSIGNED_BYTE, image);

  std::ofstream file(args.output, std::ofstream::binary);
  file << "P6 " << args.width << " " << args.height << " 255\n";
  for (int i = args.height - 1; i >= 0; --i) {
    for (unsigned int j = 0; j < args.width * 3; ++j) {
      file << image[i * args.width * 3 + j];
    }
  }
  delete[] image;
  delete painter;

  glfwDestroyWindow(offscreen_context);
  glfwTerminate();

  return 0;
}

