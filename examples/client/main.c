#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "parsec-dso.h"

#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"

#define PARSEC_APP_CLIPBOARD_MSG 7

#define WINDOW_W 1280
#define WINDOW_H 720

#if defined(_WIN32)
	#include <GL/gl.h>
	#if !defined(BITS)
		#define BITS 64
	#endif
	#if (BITS == 64)
		#define SDK_PATH "../../sdk/windows/parsec.dll"
	#else
		#define SDK_PATH "../../sdk/windows/parsec32.dll"
	#endif
#elif defined(__APPLE__)
	#define GL_SILENCE_DEPRECATION
	#include <OpenGL/gl.h>
	#define SDK_PATH "../../sdk/macos/libparsec.dylib"
#else
	#include <GL/gl.h>
	#define SDK_PATH "../../sdk/linux/libparsec.so"
#endif

struct context {
	bool done;
	float scale;
	ParsecDSO *parsec;
	SDL_Window *window;
	SDL_GLContext *glContext;
	SDL_Surface *surface;
	SDL_Cursor *cursor;
	SDL_AudioDeviceID audio;
};

static void logCallback(enum ParsecLogLevel level, char *msg, void *opaque)
{
	opaque;

	printf("[%s] %s\n", level == LOG_DEBUG ? "D" : "I", msg);
}

static void audio(int16_t *pcm, uint32_t frames, void *opaque)
{
	struct context *context = (struct context *) opaque;

	if (SDL_GetQueuedAudioSize(context->audio) < 20000)
		SDL_QueueAudio(context->audio, pcm, frames * 2 * sizeof(int16_t));
}

static void userData(struct context *context, uint32_t id, uint32_t bufferKey)
{
	char *msg = ParsecGetBuffer(context->parsec, bufferKey);

	if (msg && id == PARSEC_APP_CLIPBOARD_MSG)
		SDL_SetClipboardText(msg);

	ParsecFree(context->parsec, msg);
}

static void cursor(struct context *context, ParsecCursor *cursor, uint32_t bufferKey)
{
	if (cursor->imageUpdate) {
		uint8_t *image = ParsecGetBuffer(context->parsec, bufferKey);

		if (image) {
			SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(image, cursor->width, cursor->height,
				32, cursor->width * 4, 0xff, 0xff00, 0xff0000, 0xff000000);
			SDL_Cursor *sdlCursor = SDL_CreateColorCursor(surface, cursor->hotX, cursor->hotY);
			SDL_SetCursor(sdlCursor);

			SDL_FreeCursor(context->cursor);
			context->cursor = sdlCursor;

			SDL_FreeSurface(context->surface);
			context->surface = surface;

			ParsecFree(context->parsec, image);
		}
	}

	if (cursor->modeUpdate) {
		if (SDL_GetRelativeMouseMode() && !cursor->relative) {
			SDL_SetRelativeMouseMode(SDL_DISABLE);
			SDL_WarpMouseInWindow(context->window, cursor->positionX, cursor->positionY);

		} else if (!SDL_GetRelativeMouseMode() && cursor->relative) {
			SDL_SetRelativeMouseMode(SDL_ENABLE);
		}
	}
}

static int32_t audioThread(void *opaque)
{
	struct context *context = (struct context *) opaque;

	while (!context->done)
		ParsecClientPollAudio(context->parsec, audio, 100, context);

	return 0;
}

static int32_t renderThread(void *opaque)
{
	struct context *context = (struct context *) opaque;

	SDL_GL_SetSwapInterval(1);
#if defined(__APPLE__)
	SDL_GL_MakeCurrent(context->window, context->glContext);
#else
	context->glContext = SDL_GL_CreateContext(context->window);
#endif

	while (!context->done) {
		int32_t w = 0, h = 0;
		SDL_GetWindowSize(context->window, &w, &h);
		ParsecClientSetDimensions(context->parsec, w, h, context->scale);

		glViewport(0, 0, w, h);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		ParsecClientGLRenderFrame(context->parsec, NULL, NULL, 100);
		SDL_GL_SwapWindow(context->window);
		glFinish();
	}

	ParsecClientGLDestroy(context->parsec);
	SDL_GL_DeleteContext(context->glContext);

	return 0;
}

int32_t main(int32_t argc, char **argv)
{
	if (argc < 3) {
		printf("Usage: client sessionID peerID\n");
		return 1;
	}

	struct context context = {0};

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

	SDL_AudioSpec want = {0}, have;
	want.freq = 48000;
	want.format = AUDIO_S16;
	want.channels = 2;
	want.samples = 2048;

	context.audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	SDL_PauseAudioDevice(context.audio, 0);

	context.window = SDL_CreateWindow("Parsec Client Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		WINDOW_W, WINDOW_H, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

#if defined(__APPLE__)
	context.glContext = SDL_GL_CreateContext(context.window);
#endif

	int32_t glW = 0;
	SDL_GL_GetDrawableSize(context.window, &glW, NULL);
	context.scale = (float) glW / (float) WINDOW_W;

	ParsecStatus e = ParsecInit(NULL, NULL, SDK_PATH, &context.parsec);
	if (e != PARSEC_OK) goto except;

	ParsecSetLogCallback(context.parsec, logCallback, NULL);
	ParsecClientConnect(context.parsec, NULL, argv[1], argv[2]);

	SDL_Thread *render_thread = SDL_CreateThread(renderThread, "renderThread", &context);
	SDL_Thread *audio_thread = SDL_CreateThread(audioThread, "audioThread", &context);

	while (!context.done) {
		for (SDL_Event msg; SDL_PollEvent(&msg);) {
			ParsecMessage pmsg = {0};

			switch (msg.type) {
				case SDL_QUIT:
					context.done = true;
					break;
				case SDL_KEYDOWN:
				case SDL_KEYUP:
					pmsg.type = MESSAGE_KEYBOARD;
					pmsg.keyboard.code = (ParsecKeycode) msg.key.keysym.scancode;
					pmsg.keyboard.mod = msg.key.keysym.mod;
					pmsg.keyboard.pressed = msg.key.type == SDL_KEYDOWN;
					break;
				case SDL_MOUSEMOTION:
					pmsg.type = MESSAGE_MOUSE_MOTION;
					pmsg.mouseMotion.relative = SDL_GetRelativeMouseMode();
					pmsg.mouseMotion.x = pmsg.mouseMotion.relative ? msg.motion.xrel : msg.motion.x;
					pmsg.mouseMotion.y = pmsg.mouseMotion.relative ? msg.motion.yrel : msg.motion.y;
					break;
				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
					pmsg.type = MESSAGE_MOUSE_BUTTON;
					pmsg.mouseButton.button = msg.button.button;
					pmsg.mouseButton.pressed = msg.button.type == SDL_MOUSEBUTTONDOWN;
					break;
				case SDL_MOUSEWHEEL:
					pmsg.type = MESSAGE_MOUSE_WHEEL;
					pmsg.mouseWheel.x = msg.wheel.x;
					pmsg.mouseWheel.y = msg.wheel.y;
					break;
				case SDL_CONTROLLERBUTTONDOWN:
				case SDL_CONTROLLERBUTTONUP:
					pmsg.type = MESSAGE_GAMEPAD_BUTTON;
					pmsg.gamepadButton.id = msg.cbutton.which;
					pmsg.gamepadButton.button = msg.cbutton.button;
					pmsg.gamepadButton.pressed = msg.cbutton.type == SDL_CONTROLLERBUTTONDOWN;
					break;
				case SDL_CONTROLLERAXISMOTION:
					pmsg.type = MESSAGE_GAMEPAD_AXIS;
					pmsg.gamepadAxis.id = msg.caxis.which;
					pmsg.gamepadAxis.axis = msg.caxis.axis;
					pmsg.gamepadAxis.value = msg.caxis.value;
					break;
				case SDL_CONTROLLERDEVICEADDED:
					SDL_GameControllerOpen(msg.cdevice.which);
					break;
				case SDL_CONTROLLERDEVICEREMOVED:
					pmsg.type = MESSAGE_GAMEPAD_UNPLUG;
					pmsg.gamepadUnplug.id = msg.cdevice.which;
					SDL_GameControllerClose(SDL_GameControllerFromInstanceID(msg.cdevice.which));
					break;
				case SDL_CLIPBOARDUPDATE:
					ParsecClientSendUserData(context.parsec, PARSEC_APP_CLIPBOARD_MSG, SDL_GetClipboardText());
					break;
			}

			if (pmsg.type != 0)
				ParsecClientSendMessage(context.parsec, &pmsg);

			e = ParsecClientGetStatus(context.parsec, NULL);
			if (e != PARSEC_CONNECTING && e != PARSEC_OK)
				context.done = true;
		}

		for (ParsecClientEvent event; ParsecClientPollEvents(context.parsec, 0, &event);) {
			switch (event.type) {
				case CLIENT_EVENT_CURSOR:
					cursor(&context, &event.cursor.cursor, event.cursor.key);
					break;
				case CLIENT_EVENT_USER_DATA:
					userData(&context, event.userData.id, event.userData.key);
					break;
				case CLIENT_EVENT_RUMBLE:
					break;
				default:
					break;
			}
		}

		SDL_Delay(1);
	}

	SDL_WaitThread(audio_thread, NULL);
	SDL_WaitThread(render_thread, NULL);

	except:

	if (e != PARSEC_OK) {
		char error[32];
		snprintf(error, 32, "Parsec error: %d\n", e);
		SDL_ShowSimpleMessageBox(0, "Parsec Error", error, NULL);
	}

	ParsecDestroy(context.parsec);

	SDL_DestroyWindow(context.window);
	SDL_CloseAudioDevice(context.audio);
	SDL_Quit();

	return 0;
}
