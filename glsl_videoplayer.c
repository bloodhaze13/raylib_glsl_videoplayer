// Quick example of issue based on "raylib MPEG video player" code found within one of the github issues
// Most comments have been removed except where related to the shaders

/*******************************************************************************************
*
*   raylib MPEG video player
*
*   We have two options to decode video & audio using pl_mpeg.h library:
*
*   1) Use plm_decode() and just hand over the delta time since the last call.
*      It will decode everything needed and call your callbacks (specified through
*      plm_set_{video|audio}_decode_callback()) any number of times.
*
*   2) Use plm_decode_video() and plm_decode_audio() to decode exactly one
*      frame of video or audio data at a time. How you handle the synchronization of
*      both streams is up to you.
*
*   This example uses option 2) and handles synchonization manually.
*
*   This example has been created using raylib 3.0 (www.raylib.com)
*   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
*
*   Copyright (c) 2020 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#define PL_MPEG_IMPLEMENTATION
#include "include/pl_mpeg.h"

#define GLSL_VERSION        330
#define MAX_POSTPRO_SHADERS   3

typedef enum {
    FX_NONE = 0,
    FX_GLITCH,
    FX_SCANLINES
} PostproShader;

static const char *postproShaderText[] = {
    "NONE",
    "GLITCH",
    "SCANLINES"
};

int plm_get_total_video_frames(const char *filename) {
    int total_frames = 0;
    plm_t *plm = plm_create_with_filename(filename);
    if (plm != NULL)
    {
        plm_frame_t *frame = plm_skip_video_frame(plm);
        while (frame != NULL)
        {
            total_frames++;
            frame = plm_skip_video_frame(plm);
        }

        plm_destroy(plm);
    }
    return total_frames;
}

int plm_get_total_audio_frames(const char *filename)
{
    int total_frames = 0;
    plm_t *plm = plm_create_with_filename(filename);
    if (plm != NULL) {
        plm_samples_t *sample = plm_decode_audio(plm);
        while (sample != NULL)
        {
            total_frames++;
            sample = plm_decode_audio(plm);
        }
        plm_destroy(plm);
    }
    return total_frames;
}

int main(void)
{
    const int screenWidth = 960;
    const int screenHeight = 540;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Video Player w/ GLSL Post-Processing Shader Support");
    SetTargetFPS(500);
    InitAudioDevice();

    plm_t *plm = NULL;
    plm_frame_t *frame = NULL;
    plm_samples_t *sample = NULL;
    double framerate = 0.0;
    int samplerate = 0;

    Image imFrame = { 0 };
    Texture texture = { 0 };
    AudioStream stream = { 0 };

    double baseTime = 0.0;
    double timeExcess = 0.0;
    bool pause = false;

    int totalVideoFrames = 0;
    int currentVideoFrame = 0;

    int totalAudioFrames = 0;
    int currentAudioFrame = 0;

    Rectangle timeBar = { 0 };

    Shader shaders[MAX_POSTPRO_SHADERS] = { 0 };

    // Shader which performs no transformation
    shaders[FX_NONE] = LoadShader(0, TextFormat("shaders/glsl%i/none.fs", GLSL_VERSION));

    // Raylib included shader as working example
    shaders[FX_SCANLINES] = LoadShader(0, TextFormat("shaders/glsl%i/scanlines.fs", GLSL_VERSION));

    // Start Custom shader ported from shadertoy.com
    shaders[FX_GLITCH] = LoadShader(0, TextFormat("shaders/glsl%i/glitch.fs", GLSL_VERSION));
    int iTimeLoc = GetShaderLocation(shaders[FX_GLITCH], "iTime");
    int iResolutionLoc = GetShaderLocation(shaders[FX_GLITCH], "iResolution");

    int currentShader = FX_NONE;

    while (!WindowShouldClose()) {
        // Each frame - update iTime and iResolution to be used in GLSL shader
        float iTime = (float) GetTime();
        Vector3 iResolution = (Vector3) { GetWindowPosition().x, GetWindowPosition().y, 1.0f };
        SetShaderValue(shaders[FX_GLITCH], iTimeLoc, &iTime, UNIFORM_FLOAT);
        SetShaderValue(shaders[FX_GLITCH], iResolutionLoc, &iResolution, UNIFORM_VEC3);

        if (IsKeyPressed(KEY_RIGHT)) currentShader++;
        else if (IsKeyPressed(KEY_LEFT)) currentShader--;

        if (currentShader >= MAX_POSTPRO_SHADERS) currentShader = 0;
        else if (currentShader < 0) currentShader = MAX_POSTPRO_SHADERS - 1;

        if (IsFileDropped()) {
            int dropsCount = 0;
            char **droppedFiles = GetDroppedFiles(&dropsCount);

            if ((dropsCount == 1) && IsFileExtension(droppedFiles[0], ".mpg")) {
                if (plm != NULL) {
                    plm_destroy(plm);
                    frame = NULL;
                    sample = NULL;
                    framerate = 0.0;
                    samplerate = 0;

                    UnloadImage(imFrame);
                    UnloadTexture(texture);
                    CloseAudioStream(stream);
                    baseTime = 0.0;
                    timeExcess = 0.0;
                    pause = false;
                }

                totalVideoFrames = plm_get_total_video_frames(droppedFiles[0]);
                totalAudioFrames = plm_get_total_audio_frames(droppedFiles[0]);

                printf("Video total frames: %i\n", totalVideoFrames);
                printf("Audio total samples: %i\n", totalAudioFrames);
                plm = plm_create_with_filename(droppedFiles[0]);

                if (plm != NULL) {
                    plm_set_loop(plm, false);

                    framerate = plm_get_framerate(plm);
                    samplerate = plm_get_samplerate(plm);

                    TraceLog(LOG_INFO, "[%s] Loaded succesfully. Framerate: %f - Samplerate: %i", droppedFiles[0], (float)framerate, samplerate);

                    int videoWidth = plm_get_width(plm);
                    int videoHeight = plm_get_height(plm);
                    SetWindowSize(videoWidth, videoHeight);

                    timeBar = (Rectangle){ 0, GetScreenHeight() - 10, GetScreenWidth(), 10 };

                    imFrame.width = videoWidth;
                    imFrame.height = videoHeight;
                    imFrame.format = UNCOMPRESSED_R8G8B8;
                    imFrame.mipmaps = 1;
                    imFrame.data = (unsigned char *)malloc(imFrame.width*imFrame.height*3);
                    texture = LoadTextureFromImage(imFrame);
                    if (plm_get_num_audio_streams(plm) > 0)
                    {
                        plm_set_audio_enabled(plm, true, 0);

                        SetAudioStreamBufferSizeDefault(1152);
                        stream = InitAudioStream(samplerate, 32, 2);

                        PlayAudioStream(stream);
                    }
                }
            }
            ClearDroppedFiles();
        }
        if (IsKeyPressed(KEY_SPACE)) pause = !pause;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), timeBar)) {
            int barPositionX = GetMouseX();
            currentAudioFrame = (barPositionX*totalAudioFrames)/GetScreenWidth();
            currentVideoFrame = (int)((float)currentAudioFrame*(float)totalVideoFrames/(float)totalAudioFrames);
            if (plm != NULL) {
                plm_rewind(plm);
                frame = NULL;
                sample = NULL;
                baseTime = 0.0;
                timeExcess = 0.0;
            }
            for (int i = 0; i < currentAudioFrame; i++) sample = plm_decode_audio(plm);
            for (int i = 0; i < currentVideoFrame; i++) frame = plm_skip_video_frame(plm);
        }
        if (IsKeyPressed(KEY_R)) {
            if (plm != NULL) plm_destroy(plm);
            plm = NULL;
            frame = NULL;
            sample = NULL;
            baseTime = 0.0;
            timeExcess = 0.0;
        }

        if ((plm != NULL) && !pause) {
            double time = (GetTime() - baseTime);
            if (time >= (1.0/framerate)) {
                timeExcess += (time - 0.040);
                baseTime = GetTime();
                frame = plm_decode_video(plm);
                currentVideoFrame++;

                if (timeExcess >= 0.040) {
                    frame = plm_decode_video(plm);
                    currentVideoFrame++;
                    timeExcess = 0;
                }

                if (frame != NULL) {
                    plm_frame_to_rgb(frame, imFrame.data);
                    UpdateTexture(texture, imFrame.data);
                }
            }

            while (IsAudioStreamProcessed(stream)) {
                sample = plm_decode_audio(plm);
                currentAudioFrame++;
                if (sample != NULL) {
                    UpdateAudioStream(stream, sample->interleaved, PLM_AUDIO_SAMPLES_PER_FRAME*2);
                }
            }

            if (currentVideoFrame >= totalVideoFrames) {
                if (plm != NULL) plm_destroy(plm);
                plm = NULL;
                frame = NULL;
                sample = NULL;
                baseTime = 0.0;
                timeExcess = 0.0;
            }
        }
        BeginDrawing();
            ClearBackground(RAYWHITE);
            if (plm != NULL) {

                BeginShaderMode(shaders[currentShader]);
                    //If shader required second input texture then it must be assigned here for some reason, see:
                    //     https://github.com/raysan5/raylib/blob/8327857488404b6a242eb771e55ee7b674c61f64/examples/shaders/shaders_multi_sample2d.c#L68
                    //SetShaderValueTexture(shaders[FX_GLITCH], texture1Loc, texture1);
                    DrawTexture(texture, GetScreenWidth()/2 - texture.width/2, GetScreenHeight()/2 - texture.height/2, WHITE);
                EndShaderMode();

                DrawText("Change shader with <- ->: Current:", 200, 10, 30, DARKBLUE);
                DrawText(postproShaderText[currentShader], 10, 10, 30, RED);
                DrawText(TextFormat("CURRENT VIDEO FRAME: %i", currentVideoFrame), 10, 50, 10, LIGHTGRAY);
                DrawText(TextFormat("CURRENT AUDIO FRAME: %i", currentAudioFrame), 10, 70, 10, LIGHTGRAY);
                DrawRectangleRec(timeBar, GRAY);
                DrawRectangle(0, GetScreenHeight() - 10, (GetScreenWidth()*currentVideoFrame)/totalVideoFrames, 10, BLUE);
                if (CheckCollisionPointRec(GetMousePosition(), timeBar)) DrawRectangleLinesEx(timeBar, 1, DARKBLUE);
                if (pause) {
                    DrawRectangle(GetScreenWidth()/2 - 40, GetScreenHeight()/2 - 40, 20, 80, RAYWHITE);
                    DrawRectangle(GetScreenWidth()/2 + 10, GetScreenHeight()/2 - 40, 20, 80, RAYWHITE);
                }
            }
            else {
                DrawText("MPEG Video Player", 320, 180, 30, LIGHTGRAY);
                DrawText("Drag and drop your MPEG file", 310, 240, 20, LIGHTGRAY);
            }
        EndDrawing();
    }

    for (int i = 0; i < MAX_POSTPRO_SHADERS; i++) UnloadShader(shaders[i]);

    UnloadImage(imFrame);
    UnloadTexture(texture);

    CloseAudioStream(stream);
    CloseAudioDevice();

    if (plm != NULL) plm_destroy(plm);

    CloseWindow();
    return 0;
}
