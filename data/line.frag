#version 450

// ¡Cuidado! Asegúrate de que las 'location' coinciden con el Vertex Shader
layout( location = 0 ) in vec4 in_color;
layout( location = 1 ) in vec2 in_dst_top_left;
layout( location = 2 ) in vec2 in_dst_bottom_right;
layout( location = 3 ) in vec2 in_dst_pos;
layout( location = 4 ) in float out_border_width; // <- Debe ser location 4

layout( location = 0 ) out vec4 out_color;

// Tu función SDF (es perfecta)
// Distance from p to the line defined from a -> b
float sdLine( vec2 p, vec2 a, vec2 b, float r )
{
	vec2 pa = p - a, ba = b - a;
	float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
	return length( pa - ba*h ) - r;
}
void main()
{
    // El radio es la mitad del grosor total
    float radius = out_border_width / 2.0;
    
    // 1. Calcula la distancia
    // (Usamos un radio un poco más grande, 4.0, como tenías, para que sea visible)
    float dist = sdLine(in_dst_pos, in_dst_top_left, in_dst_bottom_right, radius);

    // 2. Convierte la distancia en un valor alfa (0.0 a 1.0)
    //    smoothstep crea un borde suave (anti-aliasing) de 1 píxel.
    //    Si dist < 0 (dentro), alpha = 1.0
    //    Si dist > 1 (fuera), alpha = 0.0
    float alpha = 1.0 - smoothstep(0.0, 1.0, dist);

    // 3. Aplica el alfa al color de entrada
    out_color = vec4(in_color.rgb, in_color.a * alpha);

    // Opcional: descarta píxeles totalmente transparentes
    //if (out_color.a < 0.01) {
    //    discard;
    //}
}