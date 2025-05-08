#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform int pass;

const int radius = 16;

void
main()
{
  vec2 dir = vec2(pass == 0, pass == 1);
  vec2 tex_size = textureSize(texture0, 0);
  vec2 tex_offset = 1.0 / tex_size;
  vec3 result = vec3(0.0);
  float weight_sum = 0.0;
  float sigma = radius * 0.5;

  for (int i = -radius; i <= radius; i++) {
    vec2 offset = dir * i * tex_offset;
    float weight = exp(-i * i / (2.0 * sigma * sigma));
    result += texture(texture0, fragTexCoord + offset).rgb * weight;
    weight_sum += weight;
  }

  result /= weight_sum;
  finalColor = vec4(result, 1.0);
}