#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xcb/xcb.h>

SDL_Surface* surface_from_screenshot() {
    uint8_t* image_bytes;
    int image_width;
    int image_height;
    int image_size;
    int image_depth;
    SDL_Surface* surf = NULL;

    xcb_connection_t* connection = xcb_connect(NULL, NULL);
    /* xorg connection creation */
    const xcb_setup_t* setup = xcb_get_setup(connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    xcb_screen_t* screen = iter.data;

    /* create gc */
    xcb_gcontext_t gc = xcb_generate_id(connection);
    {
        uint32_t mask = XCB_GC_SUBWINDOW_MODE;
        uint32_t values[] = {XCB_SUBWINDOW_MODE_INCLUDE_INFERIORS};
        xcb_create_gc(connection, gc, screen->root, mask, values);
    }

    {
        xcb_get_geometry_cookie_t cookie =
            xcb_get_geometry(connection, screen->root);
        xcb_get_geometry_reply_t* reply =
            xcb_get_geometry_reply(connection, cookie, NULL);

        image_width = reply->width;
        image_height = reply->height;
        image_depth = reply->depth;

        free(reply);
    }

    /* get image data */
    {
        xcb_get_image_cookie_t cookie =
            xcb_get_image(connection, XCB_IMAGE_FORMAT_Z_PIXMAP, screen->root,
                          0, 0, image_width, image_height, ~0);
        xcb_get_image_reply_t* reply =
            xcb_get_image_reply(connection, cookie, NULL);

        uint8_t* data = xcb_get_image_data(reply);
        image_size = xcb_get_image_data_length(reply);

        image_bytes = malloc(image_size);
        memcpy(image_bytes, data, image_size);

        free(reply);
    }

    printf("Screenshot %dx%dx%d, %p, %d\n", image_width, image_height,
           image_depth, image_bytes, image_size);

    surf = SDL_CreateRGBSurfaceFrom(image_bytes, image_width, image_height, 32,
                                    image_width * 4, 0xff0000, 0xff00, 0xff,
                                    0xff000000);

    xcb_disconnect(connection);

    return surf;
}

int main(void) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Rect display_rect;
    SDL_GetDisplayBounds(0, &display_rect);

    SDL_Window* window =
        SDL_CreateWindow("Snoop", 0, 0, display_rect.w, display_rect.h,
                         SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_FULLSCREEN);

    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    assert(renderer);

    SDL_Surface* temp_surf = surface_from_screenshot();
    assert(temp_surf);

    SDL_Texture* screen_tex = SDL_CreateTextureFromSurface(renderer, temp_surf);
    assert(screen_tex);

    int screen_width = temp_surf->w;
    int screen_height = temp_surf->h;

    SDL_FreeSurface(temp_surf);

    int running = 1;

    int start_mouse_x;
    int start_mouse_y;
    int left_pressed;

    float start_offset_x;
    float start_offset_y;

    float offset_x = 0;
    float offset_y = 0;

    float scale = 1;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT: {
                    running = 0;
                } break;
                case SDL_MOUSEBUTTONDOWN: {
                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        start_mouse_x = ev.button.x;
                        start_mouse_y = ev.button.y;
                        start_offset_x = offset_x;
                        start_offset_y = offset_y;

                        printf("pressed left\n");
                        left_pressed = 1;
                    }
                } break;

                case SDL_MOUSEWHEEL: {
                    int mx, my;
                    SDL_GetMouseState(&mx, &my);
                    float s;
                    if (ev.wheel.y > 0) {
                        s = 1.10;
                    } else {
                        s = 0.90;
                    }

                    offset_x += (offset_x - mx) * s - (offset_x - mx);
                    offset_y += (offset_y - my) * s - (offset_y - my);

                    scale *= s;
                } break;

                case SDL_MOUSEBUTTONUP: {
                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        left_pressed = 0;
                    }
                } break;

                case SDL_MOUSEMOTION: {
                    if (left_pressed) {
                        offset_x =
                            start_offset_x + (ev.motion.x - start_mouse_x);
                        offset_y =
                            start_offset_y + (ev.motion.y - start_mouse_y);
                    }
                } break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_Rect rect = {.x = offset_x,
                         .y = offset_y,
                         .w = screen_width * scale,
                         .h = screen_height * scale};
        SDL_RenderCopy(renderer, screen_tex, NULL, &rect);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(screen_tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    return 0;
}
