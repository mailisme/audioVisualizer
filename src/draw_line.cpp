#include <SDL3/SDL.h>
#include <cmath>

SDL_FColor toFColor(SDL_Color a)
{
    return SDL_FColor{
        a.r / 255.0f,
        a.g / 255.0f,
        a.b / 255.0f,
        a.a / 255.0f
    };
}



void drawThickLine(
    SDL_Renderer* renderer,
    float x1, float y1,
    float x2, float y2,
    float thickness,
    SDL_Color color
){
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = std::sqrt(dx*dx + dy*dy);

    if (len == 0) return;

    float ox = -dy / len * thickness * 0.5f;
    float oy =  dx / len * thickness * 0.5f;

    SDL_Vertex v[6];

    SDL_Vertex a;
    SDL_Vertex b;
    SDL_Vertex c;
    SDL_Vertex d;
    SDL_Vertex e;
    SDL_Vertex f;

    SDL_FPoint p1 = { x1 + ox, y1 + oy };
    SDL_FPoint p2 = { x1 - ox, y1 - oy };
    SDL_FPoint p3 = { x2 - ox, y2 - oy };
    SDL_FPoint p4 = { x2 + ox, y2 + oy };

    a.position = p1;
    a.color = toFColor(color);
    a.tex_coord = {0,0};

    b.position = p2;
    b.color = toFColor(color);
    b.tex_coord = {0,0};

    c.position = p3;
    c.color = toFColor(color);
    c.tex_coord = {0,0};

    d.position = p3;
    d.color = toFColor(color);
    d.tex_coord = {0,0};

    e.position = p4;
    e.color = toFColor(color);
    e.tex_coord = {0,0};

    f.position = p1;
    f.color = toFColor(color);
    f.tex_coord = {0,0};

    v[0] = a;
    v[1] = b;
    v[2] = c;
    v[3] = d;
    v[4] = e;
    v[5] = f;

    SDL_RenderGeometry(renderer, nullptr, v, 6, nullptr, 0);
}