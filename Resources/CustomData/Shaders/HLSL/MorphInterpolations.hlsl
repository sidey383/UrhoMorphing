float3 slerp(float3 a, float3 b, float t)
{
    float cosTheta = dot(normalize(a), normalize(b));
    // Предотвращение ошибок из-за числовых погрешностей - выход за рамки [-1, 1]
    cosTheta = clamp(cosTheta, -1.0, 1.0);
    float theta = acos(cosTheta);
    float sinTheta = sin(theta);

    // Если угол очень мал, используем линейную интерполяцию
    if (sinTheta < 0.001)
    {
        return lerp(a, b, t);
    }

    // Веса для начального и конечного положения
    float weightA = sin((1.0 - t) * theta) / sinTheta;
    float weightB = sin(t * theta) / sinTheta;

    return weightA * a + weightB * b;
}

float3 slerpMoved(float3 move, float3 a, float3 b, float t)
{
    return slerp(a - move, b - move, t) + move;
}

float3 cylerp(float3 axis, float3 from, float3 to, float weight) {
    axis = normalize(axis);

    // Поворачиваемые векторы

    // Разбиваем вектора на две части - ортогональную и колиниарную оси вращения
    float proj_from = dot(from, axis);
    float proj_to = dot(to, axis);
    float3 ortho_from = from - axis * proj_from;
    float3 ortho_to = to - axis * proj_to;

    return slerp(ortho_from, ortho_to, weight) + axis * ((1 - weight) * proj_from + weight * proj_to);
}

float3 cylerpMoved(float3 center, float3 axis, float3 from, float3 to, float weight) {
    return cylerp(axis, from - center, to - center, weight) + center;
}