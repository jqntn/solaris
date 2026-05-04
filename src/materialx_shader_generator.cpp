#include <MaterialXCore/Definition.h>
#include <MaterialXCore/Document.h>
#include <MaterialXCore/Element.h>
#include <MaterialXFormat/File.h>
#include <MaterialXFormat/Util.h>
#include <MaterialXFormat/XmlIo.h>
#include <MaterialXGenGlsl/GlslShaderGenerator.h>
#include <MaterialXGenShader/GenContext.h>
#include <MaterialXGenShader/HwShaderGenerator.h>
#include <MaterialXGenShader/Library.h>
#include <MaterialXGenShader/Shader.h>
#include <MaterialXGenShader/ShaderStage.h>
#include <MaterialXGenShader/Util.h>
#include <cctype>
#include <exception>
#include <filesystem>
#include <machina/level_description.hpp>
#include <machina/materialx_shader_generator.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace machina {
namespace {

std::string
SanitizeIdentifier(const std::string& value)
{
  std::string result;
  result.reserve(value.size());

  for (const unsigned char character : value) {
    if (std::isalnum(character) != 0 || character == '_') {
      result.push_back(static_cast<char>(character));
    } else {
      result.push_back('_');
    }
  }

  if (result.empty()) {
    return "material";
  }

  if (std::isdigit(static_cast<unsigned char>(result.front())) != 0) {
    result.insert(result.begin(), '_');
  }

  return result;
}

std::string
XmlEscaped(const std::string& value)
{
  std::string result;
  result.reserve(value.size());

  for (const char character : value) {
    switch (character) {
      case '&':
        result += "&amp;";
        break;
      case '<':
        result += "&lt;";
        break;
      case '>':
        result += "&gt;";
        break;
      case '"':
        result += "&quot;";
        break;
      case '\'':
        result += "&apos;";
        break;
      default:
        result.push_back(character);
        break;
    }
  }

  return result;
}

std::string
MaterialDocumentXml(const MaterialDescription& material)
{
  const std::string materialName = "material";
  const std::string shaderName = "material_shader";

  std::ostringstream xml;
  xml << "<?xml version=\"1.0\"?>\n";
  xml << "<materialx version=\"1.39\" colorspace=\"lin_rec709\">\n";
  xml << "  <surfacematerial name=\"" << XmlEscaped(materialName)
      << "\" type=\"material\">\n";
  xml << "    <input name=\"surfaceshader\" type=\"surfaceshader\" nodename=\""
      << XmlEscaped(shaderName) << "\" />\n";
  xml << "  </surfacematerial>\n";
  xml << "  <" << XmlEscaped(material.nodeCategory) << " name=\""
      << XmlEscaped(shaderName) << "\" type=\"" << XmlEscaped(material.nodeType)
      << "\">\n";

  for (const MaterialInput& input : material.inputs) {
    if (input.value.empty()) {
      continue;
    }

    xml << "    <input name=\"" << XmlEscaped(input.name) << "\" type=\""
        << XmlEscaped(input.type) << "\" value=\"" << XmlEscaped(input.value)
        << "\" />\n";
  }

  xml << "  </" << XmlEscaped(material.nodeCategory) << ">\n";
  xml << "</materialx>\n";

  return xml.str();
}

void
ReplaceAll(std::string& value, std::string_view from, std::string_view to)
{
  std::size_t offset = 0;
  while ((offset = value.find(from, offset)) != std::string::npos) {
    value.replace(offset, from.size(), to);
    offset += to.size();
  }
}

std::string
RaylibVertexSource(std::string source)
{
  ReplaceAll(
    source, "in vec3 i_position;", "layout(location = 0) in vec3 i_position;");
  ReplaceAll(source,
             "in vec2 i_texcoord_0;",
             "layout(location = 1) in vec2 i_texcoord_0;");
  ReplaceAll(
    source, "in vec3 i_normal;", "layout(location = 2) in vec3 i_normal;");
  ReplaceAll(
    source, "in vec3 i_tangent;", "layout(location = 4) in vec3 i_tangent;");
  ReplaceAll(source,
             "uniform mat4 u_viewProjectionMatrix = mat4(1.0);",
             "uniform mat4 u_worldViewProjectionMatrix = mat4(1.0);");
  ReplaceAll(source,
             "gl_Position = u_viewProjectionMatrix * hPositionWorld;",
             "gl_Position = u_worldViewProjectionMatrix * "
             "vec4(i_position, 1.0);");
  return source;
}

std::string
RaylibFragmentSource(std::string source)
{
  return source;
}

}

MaterialXShaderGenerator::MaterialXShaderGenerator(
  std::filesystem::path materialXRoot)
  : materialXRoot(std::move(materialXRoot))
{
}

ShaderGenerationResult
MaterialXShaderGenerator::Generate(const MaterialDescription& material) const
{
  ShaderGenerationResult result;

  try {
    MaterialX::DocumentPtr libraries = MaterialX::createDocument();
    const MaterialX::FileSearchPath searchPath(materialXRoot.string());
    MaterialX::loadLibraries(
      MaterialX::FilePathVec{ MaterialX::FilePath("libraries") },
      searchPath,
      libraries);

    MaterialX::DocumentPtr document = MaterialX::createDocument();
    MaterialX::readFromXmlString(
      document, MaterialDocumentXml(material), searchPath);
    document->importLibrary(libraries);

    std::string validation;
    if (!document->validate(&validation)) {
      result.diagnostics.push_back({ "MaterialX validation failed for " +
                                     material.path + ": " + validation });
      return result;
    }

    std::vector<MaterialX::TypedElementPtr> renderables =
      MaterialX::findRenderableElements(document);
    if (renderables.empty()) {
      result.diagnostics.push_back(
        { "MaterialX generation found no renderable element for " +
          material.path });
      return result;
    }

    MaterialX::ShaderGeneratorPtr generator =
      MaterialX::GlslShaderGenerator::create();
    MaterialX::GenContext context(generator);
    context.registerSourceCodeSearchPath(searchPath);

    MaterialX::NodeDefPtr directionalLight =
      document->getNodeDef("ND_directional_light");
    if (directionalLight) {
      MaterialX::HwShaderGenerator::bindLightShader(
        *directionalLight, 1, context);
    }

    MaterialX::ShaderPtr shader =
      generator->generate("material", renderables.front(), context);

    result.shader.vertexSource =
      RaylibVertexSource(shader->getSourceCode(MaterialX::Stage::VERTEX));
    result.shader.fragmentSource =
      RaylibFragmentSource(shader->getSourceCode(MaterialX::Stage::PIXEL));

    if (result.shader.vertexSource.empty() ||
        result.shader.fragmentSource.empty()) {
      result.diagnostics.push_back(
        { "MaterialX generated empty GLSL for " + material.path });
    }
  } catch (const std::exception& exception) {
    result.diagnostics.push_back({ "MaterialX generation failed for " +
                                   material.path + ": " + exception.what() });
  }

  return result;
}

}
