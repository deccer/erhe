out vec2      v_texcoord;
out vec4      v_position;
out vec4      v_color;
out mat3      v_TBN;
out flat uint v_material_index;

void main()
{
    mat4 world_from_node = primitive.primitives[gl_DrawID].world_from_node;
    mat4 clip_from_world = camera.cameras[0].clip_from_world;

    vec3 normal      = normalize(vec3(world_from_node * vec4(a_normal,        0.0)));
    vec3 tangent     = normalize(vec3(world_from_node * vec4(a_tangent.xyz,   0.0)));
    vec3 bitangent   = normalize(vec3(world_from_node * vec4(a_bitangent.xyz, 0.0)));
    //vec3 bitangent = normalize(cross(normal, tangent)) * a_tangent.w;
    vec4 position    = world_from_node * vec4(a_position, 1.0);

    v_TBN            = mat3(tangent, bitangent, normal);

    v_position       = position;
    gl_Position      = clip_from_world * position;
    v_material_index = primitive.primitives[gl_DrawID].material_index;
    v_texcoord       = a_texcoord;
    v_color          = a_color;
}
