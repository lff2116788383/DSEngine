// Pre-compensate sRGB display-ready color so that after ACES tonemapping + gamma

// the final pixel matches the intended display value.

// Given: final = pow(ACES(x), 1/2.2) should equal 'srgb'

// So: ACES(x) = pow(srgb, 2.2) → x = InverseACES(pow(srgb, 2.2))

vec3 InverseTonemapForDisplay(vec3 srgb) {

    vec3 y = pow(clamp(srgb, 0.001, 0.999), vec3(2.2)); // target linear after ACES

    // Solve ACES: y = (x*(2.51x+0.03))/(x*(2.43x+0.59)+0.14)

    // → (2.43y-2.51)x² + (0.59y-0.03)x + 0.14y = 0

    vec3 A = 2.43 * y - 2.51;

    vec3 B = 0.59 * y - 0.03;

    vec3 C = 0.14 * y;

    vec3 disc = max(B * B - 4.0 * A * C, vec3(0.0));

    return (-B - sqrt(disc)) / (2.0 * A);

}



void OutputFragment(vec3 color, float alpha) {

    if (u_wboit_mode > 0.5) {

        float z = gl_FragCoord.z;

        float w = alpha * max(1e-2, 3e3 * pow(1.0 - z, 3.0));

        if (u_wboit_mode < 1.5) {

            FragColor = vec4(color * alpha * w, alpha * w);

        } else {

            FragColor = vec4(0.0, 0.0, 0.0, alpha);

        }

        return;

    }

    FragColor = vec4(color, alpha);

}
