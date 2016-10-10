#include <pebble.h>
	
#define LANG_1 0
#define LANG_2 1
#define RAW_TEXT 2
#define TRANSLATED_TEXT 3
	
char spoken_lang_list[27][16] = {
	"Albanian",
	"Bosnian",
	"Catalan",
	"Croatian",
	"Czech",
	"Danish",
	"Dutch",
	"English",
	"Estonian",
	"Finnish",
	"French",
	"German",
	"Hungarian",
	"Icelandic",
	"Italian",
	"Latvian",
	"Lithuanian",
	"Malay",
	"Maltese",
	"Norwegian",
	"Polish",
	"Portuguese",
	"Romanian",
	"Spanish",
	"Slovak",
	"Swedish",
	"Turkish"
};

char spoken_lang_abbr[27][2] = {
	"sq",
	"bs",
	"ca",
	"hr",
	"cs",
	"da",
	"nl",
	"en",
	"et",
	"fi",
	"fr",
	"de",
	"hu",
	"is",
	"it",
	"lv",
	"lt",
	"ms",
	"mt",
	"no",
	"pl",
	"pt",
	"ro",
	"es",
	"sk",
	"sv",
	"tr"
};
	
Window *input_window, *translation_window, *results_window;
Layer *spoken_lang_slider_layer, *translated_lang_slider_layer, *microphone_slider_layer, *loading_layer, *results_layer;
MenuLayer *spoken_lang_menu_layer, *translated_lang_menu_layer;

ActionBarLayer *result_menu;

char instruction_text[128];

Layer *splash_layer;

int current = 0;
int INTRO = 0, SPOKEN = 1, TRANSLATED = 2, MIC = 3, LOADING = 4, RESULTS = 5;

GRect loading_rect;
AppTimer *loading_timer;

bool animating = false;

bool first_animation = true;

bool detected = true;

GBitmap *mic, *restart, *quit, *arrow;

char lang_1[3], lang_2[3], raw_text[200];

char detection_string[32], two_lang_string[32];

char translated_text[200];

char lang_full_name[16];

int height = 0, width_3;

int MAX_COUNT;

GTextAttributes *instruct_attr;

void on_animation_stopped(Animation *anim, bool finished, void *context){
	animating = false;	
}	

void animate_layer(Layer *layer, GRect start, GRect finish, AnimationCurve curve){
	animating = true;
	
	PropertyAnimation *anim = property_animation_create_layer_frame(layer, &start, &finish);
	animation_set_duration((Animation*) anim, 200);
	
	AnimationHandlers handlers = {
		.stopped = (AnimationStoppedHandler) on_animation_stopped
	};
	
	animation_set_curve((Animation*) anim, curve);
	animation_set_handlers((Animation*) anim, handlers, NULL);
	animation_schedule((Animation*) anim);
}

bool loading_timer_active = false;
bool loading_color = true;

char mic_text[128];
bool hide_arrow = false;

DictationSession *session;

int restart_x = 0, mic_x = 0, quit_x = 0;

void loading_timer_run(){
	if(loading_color){
		loading_rect.size.w++;
		if(loading_rect.size.w == 80) loading_color = false;
	}
	else{
		loading_rect.size.w--;
		loading_rect.origin.x++;
		if(loading_rect.size.w == 0){ loading_color = true; loading_rect.origin.x = PBL_IF_RECT_ELSE(32,50); }
	}
	layer_mark_dirty(loading_layer);
	if(loading_timer_active) loading_timer = app_timer_register(13, loading_timer_run, NULL);
}

void loading_timer_start(){
	loading_timer_active = true;
	loading_rect.size.w = 0;
	loading_rect.origin.x = PBL_IF_RECT_ELSE(32,50);
	loading_timer = app_timer_register(13, loading_timer_run, NULL);
}

void loading_timer_cancel(){
	if(loading_timer != NULL) app_timer_cancel(loading_timer);
	loading_timer_active = false;
	loading_color = true;
}

void send_request(char *sr_lang_1, char *sr_lang_2, char *sr_raw_text){
	Tuplet lang_1_tuplet = TupletCString(LANG_1, sr_lang_1);
	Tuplet lang_2_tuplet = TupletCString(LANG_2, sr_lang_2);
	Tuplet raw_text_tuplet = TupletCString(RAW_TEXT, sr_raw_text);
	DictionaryIterator *request_dict;
	app_message_outbox_begin(&request_dict);
	dict_write_tuplet(request_dict, &lang_1_tuplet);
	dict_write_tuplet(request_dict, &lang_2_tuplet);
	dict_write_tuplet(request_dict, &raw_text_tuplet);
	dict_write_end(request_dict);
	app_message_outbox_send();
}

void inbox(DictionaryIterator *iter, void *context){
	if(current == LOADING){
		Tuple *t = dict_read_first(iter);
		if(t->key == LANG_1){
			strncpy(lang_full_name, "XXX", sizeof(lang_full_name));
			for(int i = 0; i < 27; i++){
				if(t->value->cstring[0] == spoken_lang_abbr[i][0] && t->value->cstring[1] == spoken_lang_abbr[i][1]){ strncpy(lang_full_name, spoken_lang_list[i], sizeof(lang_full_name)); i = 28; }
			}
			snprintf(two_lang_string, sizeof(two_lang_string), "%s -> %s", lang_full_name, spoken_lang_list[menu_layer_get_selected_index(translated_lang_menu_layer).row]);
			detected = true;
			layer_mark_dirty(loading_layer);
		}
		else if(t->key == TRANSLATED_TEXT){
			strncpy(translated_text, t->value->cstring, sizeof(translated_text));
			layer_mark_dirty(results_layer);
     		window_stack_push(results_window, true);
			current = RESULTS;
			loading_timer_cancel();
		}
	}
}

void select_click(ClickRecognizerRef ref, void *context){
	if(first_animation || animating || loading_timer_active) return;
	
	if(current == INTRO){
		animate_layer(spoken_lang_slider_layer, PBL_IF_RECT_ELSE(GRect(114,0,124,168), GRect(150,0,160,180)), PBL_IF_RECT_ELSE(GRect(0,0,124,168),GRect(0,0,160,180)), AnimationCurveEaseOut);
		current = SPOKEN;
	}
	else if(current == SPOKEN){
		if(menu_layer_get_selected_index(spoken_lang_menu_layer).row == 0){
			detected = false;
			strncpy(lang_1, "DT", sizeof(lang_1));
		}
		else{ 
			detected = true;
			snprintf(lang_1, sizeof(lang_1), "%s", spoken_lang_abbr[menu_layer_get_selected_index(spoken_lang_menu_layer).row - 1]);
		}
		persist_write_int(LANG_1, menu_layer_get_selected_index(spoken_lang_menu_layer).row);
		animate_layer(translated_lang_slider_layer, PBL_IF_RECT_ELSE(GRect(124,0,124,168), GRect(160,0,160,180)), PBL_IF_RECT_ELSE(GRect(10,0,124,168),GRect(10,0,160,180)), AnimationCurveEaseOut);
		current = TRANSLATED;
	}
	else if(current == TRANSLATED){
		persist_write_int(LANG_2, menu_layer_get_selected_index(translated_lang_menu_layer).row);
		animate_layer(microphone_slider_layer, PBL_IF_RECT_ELSE(GRect(134,0,124,168), GRect(170,0,160,180)), PBL_IF_RECT_ELSE(GRect(20,0,124,168),GRect(20,0,160,180)), AnimationCurveEaseOut);
		current = MIC;
		snprintf(lang_2, sizeof(lang_2), "%s", spoken_lang_abbr[menu_layer_get_selected_index(translated_lang_menu_layer).row]);
		if(!detected){
			snprintf(detection_string, sizeof(detection_string), "??? -> %s", spoken_lang_list[menu_layer_get_selected_index(translated_lang_menu_layer).row]);
		}
		else{
			snprintf(two_lang_string, sizeof(two_lang_string), "%s -> %s", spoken_lang_list[menu_layer_get_selected_index(spoken_lang_menu_layer).row-1], spoken_lang_list[menu_layer_get_selected_index(translated_lang_menu_layer).row]);
		}
	}
	else if(current == MIC){
		dictation_session_start(session);
	}
}

void back_click(ClickRecognizerRef ref, void *context){
	if(animating) return;
	
	if(current == INTRO){
		window_stack_pop_all(false);
	}
	else if(current == SPOKEN){
		animate_layer(spoken_lang_slider_layer, PBL_IF_RECT_ELSE(GRect(0,0,124,168),GRect(0,0,160,180)), PBL_IF_RECT_ELSE(GRect(114,0,124,168),GRect(150,0,160,180)), AnimationCurveEaseOut);
		current = INTRO;
	}
	else if(current == TRANSLATED){
		animate_layer(translated_lang_slider_layer, PBL_IF_RECT_ELSE(GRect(10,0,124,168),GRect(10,0,160,180)), PBL_IF_RECT_ELSE(GRect(124,0,124,168),GRect(160,0,160,180)), AnimationCurveEaseOut);
		current = SPOKEN;
	}
	else if(current == MIC){
		animate_layer(microphone_slider_layer, PBL_IF_RECT_ELSE(GRect(20,0,124,168),GRect(20,0,160,180)), PBL_IF_RECT_ELSE(GRect(134,0,124,168),GRect(170,0,160,180)), AnimationCurveEaseOut);
		current = TRANSLATED;
	}
	else if(current == LOADING){
		loading_timer_cancel();
		window_stack_pop(translation_window);
		current = MIC;
		
		if(menu_layer_get_selected_index(spoken_lang_menu_layer).row == 0) detected = false;
	}
	else if(current == RESULTS){
		//layer_set_frame(microphone_slider_layer, PBL_IF_RECT_ELSE(GRect(134,0,124,168),GRect(170,0,160,180)));
		window_stack_pop(results_window);
		window_stack_pop(translation_window);
		//layer_set_frame(results_layer, GRect(144,16,144,152));
		current = MIC;
    	strncpy(translated_text, "", sizeof(translated_text));
		
		if(menu_layer_get_selected_index(spoken_lang_menu_layer).row == 0) detected = false;
	}
}

void down_click(ClickRecognizerRef ref, void *context){
	if(animating || loading_timer_active) return;
	
	if(current == SPOKEN){
		if(click_number_of_clicks_counted(ref) > 1)
			menu_layer_set_selected_next(spoken_lang_menu_layer, false, MenuRowAlignCenter, false);
		else
			menu_layer_set_selected_next(spoken_lang_menu_layer, false, MenuRowAlignCenter, true);
	}
	else if(current == TRANSLATED){
		if(click_number_of_clicks_counted(ref) > 1)
			menu_layer_set_selected_next(translated_lang_menu_layer, false, MenuRowAlignCenter, false);
		else
			menu_layer_set_selected_next(translated_lang_menu_layer, false, MenuRowAlignCenter, true);
	}
}

void up_click(ClickRecognizerRef ref, void *context){
	if(animating || loading_timer_active) return;
	
	if(current == SPOKEN){
		if(click_number_of_clicks_counted(ref) > 1)
			menu_layer_set_selected_next(spoken_lang_menu_layer, true, MenuRowAlignCenter, false);
		else
			menu_layer_set_selected_next(spoken_lang_menu_layer, true, MenuRowAlignCenter, true);
	}
	else if(current == TRANSLATED){
		if(click_number_of_clicks_counted(ref) > 1)
			menu_layer_set_selected_next(translated_lang_menu_layer, true, MenuRowAlignCenter, false);
		else
			menu_layer_set_selected_next(translated_lang_menu_layer, true, MenuRowAlignCenter, true);
	}	
}

void click_config(void *context){
	window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
	window_single_click_subscribe(BUTTON_ID_BACK, back_click);
	window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, down_click);
	window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, up_click);
}

void unselect_click(ClickRecognizerRef ref, void *context){
	if(animating || loading_timer_active) return;
	
	if(current == RESULTS){
		window_stack_pop(results_window);
    	window_stack_pop(translation_window);
    	strncpy(translated_text, "", sizeof(translated_text));
		layer_mark_dirty(results_layer);
		current = MIC;
		
		if(menu_layer_get_selected_index(spoken_lang_menu_layer).row == 0) detected = false;
	}
}

void undown_click(ClickRecognizerRef ref, void *context){
	if(animating || loading_timer_active) return;
	
	if(current == RESULTS){
		window_stack_pop_all(false);
	}
}

void unup_click(ClickRecognizerRef ref, void *context){
	if(animating || loading_timer_active) return;
	
	if(current == RESULTS){
		layer_mark_dirty(results_layer);
			
		window_stack_pop(results_window);
    	window_stack_pop(translation_window);
    	strncpy(translated_text, "", sizeof(translated_text));
		layer_mark_dirty(results_layer);

		layer_set_frame(microphone_slider_layer, PBL_IF_RECT_ELSE(GRect(134,0,124,168),GRect(170,0,160,180)));
		layer_set_frame(translated_lang_slider_layer, PBL_IF_RECT_ELSE(GRect(124,0,124,168),GRect(160,0,160,180)));
		layer_set_frame(spoken_lang_slider_layer, PBL_IF_RECT_ELSE(GRect(114,0,124,168),GRect(150,0,160,180)));
		current = INTRO;
	}
}

void second_click_config(void *context){
	window_single_click_subscribe(BUTTON_ID_SELECT, unselect_click);
	window_single_click_subscribe(BUTTON_ID_DOWN, undown_click);
	window_single_click_subscribe(BUTTON_ID_UP, unup_click);
	window_single_click_subscribe(BUTTON_ID_BACK, back_click);
}

int lb_x = 0, lb_w = 48, db_x = 48, db_w = 48, g_x = 96, g_w = 48;

int slide_tick = 0;

void slider_anim(){
	animating = true;
	slide_tick++;
	lb_x += 9;
	lb_w -= 3;
	db_x += 6;
	db_w -= 3;
	g_x += 3;
	g_w -= 3;
	layer_mark_dirty(splash_layer);
	
	if(slide_tick == MAX_COUNT){ animating = false; first_animation = false; layer_destroy(splash_layer); }
	else app_timer_register(35, slider_anim, NULL);
}

void draw_splash(Layer *layer, GContext *ctx){
	graphics_context_set_fill_color(ctx, GColorLightGray);
	graphics_fill_rect(ctx, GRect(g_x, 0, g_w, height), 0, GCornerNone);
	
	graphics_context_set_fill_color(ctx, GColorCobaltBlue);
	graphics_fill_rect(ctx, GRect(db_x, 0, db_w, height), 0, GCornerNone);
	
	graphics_context_set_fill_color(ctx, GColorPictonBlue);
	graphics_fill_rect(ctx, GRect(lb_x, 0, lb_w, height), 0, GCornerNone);
	
	if(animating) return;
	
	graphics_context_set_text_color(ctx, GColorBlack);
	graphics_draw_text(ctx, "V", fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK), PBL_IF_RECT_ELSE(GRect(lb_x, 72-15, lb_w, 24), GRect(lb_x, 90-16-3, lb_w, 24)), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	graphics_draw_text(ctx, "X", fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK), PBL_IF_RECT_ELSE(GRect(g_x, 72-15, g_w, 24),GRect(g_x, 90-16-3, g_w, 24)), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	
	graphics_context_set_text_color(ctx, GColorWhite);
	graphics_draw_text(ctx, "O", fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK), PBL_IF_RECT_ELSE(GRect(db_x-1, 72-15, db_w+2,24), GRect(db_x-1, 90-16-3, db_w+2, 24)), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);		
}

void draw_instructions(Layer *layer, GContext *ctx){
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_context_set_text_color(ctx, GColorWhite);
	
	graphics_fill_rect(ctx, PBL_IF_RECT_ELSE(GRect(0,0,144,168),GRect(0,0,180,180)),0,GCornerNone);
	graphics_draw_text(ctx, instruction_text,fonts_get_system_font(FONT_KEY_GOTHIC_24), PBL_IF_RECT_ELSE(GRect(2,2,110,164),GRect(2,2,150,180)), GTextOverflowModeWordWrap, GTextAlignmentLeft, instruct_attr);
}

void draw_spoken_lang_layer(Layer *layer, GContext *ctx){
	graphics_context_set_fill_color(ctx, GColorPictonBlue);
	graphics_fill_rect(ctx, PBL_IF_RECT_ELSE(GRect(0,0,10,168),GRect(0,0,10,180)),0,GCornerNone);
	
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_rect(ctx, PBL_IF_RECT_ELSE(GRect(10,0,114,168),GRect(10,0,150,180)),0,GCornerNone);
}

static uint16_t spoken_get_num_rows(MenuLayer *menu_layer, uint16_t section, void *data){
	return 28;
}

void spoken_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *index, void *data){
	if(index->row == 0) menu_cell_basic_draw(ctx, cell_layer, "Detect", NULL, NULL);
	else menu_cell_basic_draw(ctx, cell_layer, spoken_lang_list[index->row-1], NULL, NULL);
}

void draw_translated_lang_layer(Layer *layer, GContext *ctx){
	graphics_context_set_fill_color(ctx, GColorCobaltBlue);
	graphics_fill_rect(ctx, PBL_IF_RECT_ELSE(GRect(0,0,10,168),GRect(0,0,10,180)),0,GCornerNone);

	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_rect(ctx, PBL_IF_RECT_ELSE(GRect(10,0,114,168),GRect(10,0,150,180)),0,GCornerNone);
}

static uint16_t translated_get_num_rows(MenuLayer *menu_layer, uint16_t section, void *data){
	return 27;	
}

void translated_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *index, void *data){
	menu_cell_basic_draw(ctx, cell_layer, spoken_lang_list[index->row], NULL, NULL);
}
	
void draw_microphone_layer(Layer *layer, GContext *ctx){
	graphics_context_set_fill_color(ctx, GColorLightGray);
	graphics_fill_rect(ctx, PBL_IF_RECT_ELSE(GRect(0,0,10,168),GRect(0,0,10,180)),0,GCornerNone);
	
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_rect(ctx, PBL_IF_RECT_ELSE(GRect(10,0,114,168),GRect(10,0,150,180)),0,GCornerNone);	
	
	graphics_context_set_text_color(ctx, GColorDarkGray);
	graphics_draw_text(ctx, mic_text,fonts_get_system_font(FONT_KEY_GOTHIC_24), PBL_IF_RECT_ELSE(GRect(10+2,0,100,168),GRect(10+2,0,130,180)),GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, instruct_attr);
}

void draw_loading_layer(Layer *layer, GContext *ctx){
	graphics_context_set_fill_color(ctx, GColorLightGray);
	graphics_fill_rect(ctx, PBL_IF_RECT_ELSE(GRect(0,0,144,168),GRect(0,0,180,180)),0,GCornerNone);
	
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx, PBL_IF_RECT_ELSE(GRect(32,88-16,80,6),GRect(50,88-16+12,80,6)), 2, GCornersAll);
	
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_rect(ctx, loading_rect,2,GCornersAll);
	
	graphics_context_set_text_color(ctx, GColorBlack);
	if(detected){	
		graphics_draw_text(ctx, "Translating...", fonts_get_system_font(FONT_KEY_GOTHIC_24), PBL_IF_RECT_ELSE(GRect(0, 32-16, 144,24),GRect(0, 32-16+12, 180,24)), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
		graphics_draw_text(ctx, two_lang_string, fonts_get_system_font(FONT_KEY_GOTHIC_24), PBL_IF_RECT_ELSE(GRect(0, 120-16, 144,24),GRect(0, 120-16+12, 180,24)), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	}
	else{
		graphics_draw_text(ctx, "Detecting...", fonts_get_system_font(FONT_KEY_GOTHIC_24), PBL_IF_RECT_ELSE(GRect(0, 32-16, 144,24),GRect(0, 32-16+12, 180, 24)), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
		graphics_draw_text(ctx, detection_string, fonts_get_system_font(FONT_KEY_GOTHIC_24), PBL_IF_RECT_ELSE(GRect(0, 120-16, 144,24),GRect(0, 120-16+12, 180,24)), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	}
}

void draw_results_layer(Layer *layer, GContext *ctx){
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_rect(ctx, PBL_IF_RECT_ELSE(GRect(0,0,144,168),GRect(0,0,180,180)),0,GCornerNone);
	
	graphics_context_set_text_color(ctx, GColorBlack);
	graphics_draw_text(ctx, translated_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), PBL_IF_RECT_ELSE(GRect(2,10,100,168),GRect(2,10,140,180)),GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, instruct_attr);
}

char *sys_locale;

void dictation_callback(DictationSession *session, DictationSessionStatus status, char *transcription, void *context){
	if(status == DictationSessionStatusSuccess){
		window_stack_push(translation_window, true);
		current = LOADING;
		loading_timer_start();

		strncpy(mic_text, "Press 'Select' when you are ready, and speak into the mic...", sizeof(mic_text));
		
		send_request(lang_1, lang_2, transcription);
	}
	else{
		switch(status){
			case DictationSessionStatusFailureTranscriptionRejected:			strncpy(mic_text, "Transcription Rejected!\n\nPress 'Select' when you are ready...", sizeof(mic_text)); break;
			
			case DictationSessionStatusFailureTranscriptionRejectedWithError:				strncpy(mic_text, "Transcription Error!\n\nPress 'Select' when you are ready...", sizeof(mic_text)); break;
			
			case DictationSessionStatusFailureNoSpeechDetected:	strncpy(mic_text, "No Speech Detected!\n\nPress 'Select' when you are ready...", sizeof(mic_text)); break;
			
			case DictationSessionStatusFailureConnectivityError:				strncpy(mic_text, "Connectivity Error!\n\nMake sure your Pebble is connected.", sizeof(mic_text)); break;
			
			case DictationSessionStatusFailureDisabled:							strncpy(mic_text, "Disabled!\n\nTo use this app, you must enable Voice Transcription.", sizeof(mic_text)); break;
			
			case DictationSessionStatusFailureInternalError:					strncpy(mic_text, "Internal Error!\n\nSomething went wrong. Try restarting your watch.", sizeof(mic_text)); break;
			
			default:															strncpy(mic_text, "Unknown Error!\n\nSomething went wrong. Try restarting the app.", sizeof(mic_text)); break;
		}
		hide_arrow = true;
		layer_mark_dirty(microphone_slider_layer);
	}	
}

void init(){
	
	session = dictation_session_create(0, dictation_callback, NULL);
    dictation_session_enable_confirmation(session, true);
	
	first_animation = true;
	
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
	app_message_register_inbox_received(inbox);
	
	sys_locale = setlocale(LC_ALL, "");
	if(strcmp(sys_locale, "fr_FR") == 0){
		hide_arrow = true;
		strncpy(instruction_text, "Choisissez la langue parlée.\nChoisissez la langue désirée.\nParlez dans le micro.", sizeof(instruction_text));
		strncpy(mic_text, "Appuyez sur 'Select' lorsque vous êtes prêt, et de parler dans le mic...", sizeof(mic_text));
	}
	else if(strcmp(sys_locale, "de_DE") == 0){
		hide_arrow = true;
		strncpy(instruction_text, "Wählen Sie eine Sprache.\nWählen zweite Sprache.\nSprechen Sie in das Mikrofon.", sizeof(instruction_text));
		strncpy(mic_text, "Drücken Sie 'Select', wenn Sie bereit sind, und sprechen Sie in das Mik...", sizeof(mic_text));
	}
	else if(strcmp(sys_locale, "es_ES") == 0){
		strncpy(instruction_text, "Elija idioma hablado.\nElija idioma deseado.\nHable en el micrófono.", sizeof(instruction_text));
		strncpy(mic_text, "Pulse 'Select' cuando esté listo, y hable en el mic...", sizeof(mic_text));
	}
	else{
		strncpy(instruction_text, "Choose spoken language.\nChoose desired language.\nSpeak into the microphone.", sizeof(instruction_text));	
		strncpy(mic_text, "Press 'Select' when you are ready, and speak into the mic...", sizeof(mic_text));
	}
	
	input_window = window_create();
	layer_set_update_proc(window_get_root_layer(input_window), draw_instructions);
	window_set_click_config_provider(input_window, click_config);

  width_3 = PBL_IF_RECT_ELSE(48, 60);
  height = PBL_IF_RECT_ELSE(168,180);
  lb_x = 0;
  lb_w = width_3;
  db_x = lb_w;
  db_w = width_3;
  g_x = db_x + db_w;
  g_w = width_3;
  
  instruct_attr = graphics_text_attributes_create();
  graphics_text_attributes_enable_screen_text_flow(instruct_attr, 8);
  
  MAX_COUNT = PBL_IF_RECT_ELSE(13,17);
    
	spoken_lang_slider_layer = layer_create(PBL_IF_RECT_ELSE(GRect(114,0,124,168), GRect(150,0,160,180)));
	layer_set_update_proc(spoken_lang_slider_layer, draw_spoken_lang_layer);
	
	spoken_lang_menu_layer = menu_layer_create(PBL_IF_RECT_ELSE(GRect(10,0,114,168), GRect(10,0,150,180)));
	menu_layer_set_callbacks(spoken_lang_menu_layer, NULL, (MenuLayerCallbacks){
		.get_num_rows = spoken_get_num_rows,
		.draw_row = spoken_draw_row
	});
	menu_layer_set_normal_colors(spoken_lang_menu_layer, GColorWhite, GColorCobaltBlue);
	menu_layer_set_highlight_colors(spoken_lang_menu_layer, GColorPictonBlue, GColorBlack);
	menu_layer_pad_bottom_enable(spoken_lang_menu_layer, false);
	
	layer_add_child(spoken_lang_slider_layer, menu_layer_get_layer(spoken_lang_menu_layer));
	
	layer_add_child(window_get_root_layer(input_window), spoken_lang_slider_layer);
	
	translated_lang_slider_layer = layer_create(PBL_IF_RECT_ELSE(GRect(124,0,124,168), GRect(160,0,160,180)));
	layer_set_update_proc(translated_lang_slider_layer, draw_translated_lang_layer);
	
	translated_lang_menu_layer = menu_layer_create(PBL_IF_RECT_ELSE(GRect(10,0,114,168), GRect(10,0,150,180)));
	menu_layer_set_callbacks(translated_lang_menu_layer, NULL, (MenuLayerCallbacks){
		.get_num_rows = translated_get_num_rows,
		.draw_row = translated_draw_row
	});
	menu_layer_set_normal_colors(translated_lang_menu_layer, GColorWhite, GColorCobaltBlue);
	menu_layer_set_highlight_colors(translated_lang_menu_layer, GColorCobaltBlue, GColorWhite);
	menu_layer_pad_bottom_enable(translated_lang_menu_layer, false);
	
	layer_add_child(translated_lang_slider_layer, menu_layer_get_layer(translated_lang_menu_layer));
	
	layer_add_child(window_get_root_layer(input_window), translated_lang_slider_layer);
	
	if(persist_exists(LANG_1)) menu_layer_set_selected_index(spoken_lang_menu_layer, MenuIndex(0, persist_read_int(LANG_1)), MenuRowAlignCenter, false);
	if(persist_exists(LANG_2)) menu_layer_set_selected_index(translated_lang_menu_layer, MenuIndex(0, persist_read_int(LANG_2)), MenuRowAlignCenter, false);
	
	microphone_slider_layer = layer_create(PBL_IF_RECT_ELSE(GRect(134,0,124,168), GRect(170,0,160,180)));
	layer_set_update_proc(microphone_slider_layer, draw_microphone_layer);
	
	layer_add_child(window_get_root_layer(input_window), microphone_slider_layer);
	
	splash_layer = layer_create(PBL_IF_RECT_ELSE(GRect(0,0,144,168),GRect(0,0,180,180)));
	layer_set_update_proc(splash_layer, draw_splash);
	
	layer_add_child(window_get_root_layer(input_window), splash_layer);
	
	window_stack_push(input_window, false);
	
	app_timer_register(1500, slider_anim, NULL);
	
	translation_window = window_create();
	window_set_click_config_provider(translation_window, second_click_config);
	
	loading_layer = layer_create(PBL_IF_RECT_ELSE(GRect(0,0,144,168),GRect(0,0,180,180)));
	layer_set_update_proc(loading_layer, draw_loading_layer);
	
	loading_rect = PBL_IF_RECT_ELSE(GRect(32,88-16,0,6), GRect(50,88-16+12,0,6));
	
	layer_add_child(window_get_root_layer(translation_window), loading_layer);
	
  results_window = window_create();
                               
	results_layer = layer_create(PBL_IF_RECT_ELSE(GRect(0,0,144,168), GRect(0,0,180,180)));
	layer_set_update_proc(results_layer, draw_results_layer);
                               
  layer_add_child(window_get_root_layer(results_window), results_layer);
  
  result_menu = action_bar_layer_create();
  action_bar_layer_set_click_config_provider(result_menu, second_click_config);
  action_bar_layer_set_icon(result_menu, BUTTON_ID_UP, gbitmap_create_with_resource(RESOURCE_ID_RESTART));
  action_bar_layer_set_icon(result_menu, BUTTON_ID_SELECT, gbitmap_create_with_resource(RESOURCE_ID_MIC));
  action_bar_layer_set_icon(result_menu, BUTTON_ID_DOWN, gbitmap_create_with_resource(RESOURCE_ID_QUIT));
  action_bar_layer_add_to_window(result_menu, results_window);                          
	
	if(launch_reason() == APP_LAUNCH_QUICK_LAUNCH){
		animate_layer(spoken_lang_slider_layer, PBL_IF_RECT_ELSE(GRect(114,0,124,168), GRect(150,0,160,180)), PBL_IF_RECT_ELSE(GRect(0,0,124,168),GRect(0,0,160,180)), AnimationCurveEaseOut);
		animate_layer(translated_lang_slider_layer, PBL_IF_RECT_ELSE(GRect(124,0,124,168), GRect(160,0,160,180)), PBL_IF_RECT_ELSE(GRect(10,0,124,168),GRect(10,0,160,180)), AnimationCurveEaseOut);
		animate_layer(microphone_slider_layer, PBL_IF_RECT_ELSE(GRect(134,0,124,168), GRect(170,0,160,180)), PBL_IF_RECT_ELSE(GRect(20,0,124,168),GRect(20,0,160,180)), AnimationCurveEaseOut);
		current = MIC;
		
		if(menu_layer_get_selected_index(spoken_lang_menu_layer).row == 0){
			detected = false;
			strncpy(lang_1, "DT", sizeof(lang_1));
		}
		else{ 
			detected = true;
			snprintf(lang_1, sizeof(lang_1), "%s", spoken_lang_abbr[menu_layer_get_selected_index(spoken_lang_menu_layer).row - 1]);
		}
		
		snprintf(lang_2, sizeof(lang_2), "%s", spoken_lang_abbr[menu_layer_get_selected_index(translated_lang_menu_layer).row]);
		
		if(!detected){
			snprintf(detection_string, sizeof(detection_string), "??? -> %s", spoken_lang_list[menu_layer_get_selected_index(translated_lang_menu_layer).row]);
		}
		else{
			snprintf(two_lang_string, sizeof(two_lang_string), "%s -> %s", spoken_lang_list[menu_layer_get_selected_index(spoken_lang_menu_layer).row-1], spoken_lang_list[menu_layer_get_selected_index(translated_lang_menu_layer).row]);
		}
		
		dictation_session_start(session);
	}
}

void deinit(){
	dictation_session_destroy(session);
	
	app_message_deregister_callbacks();
	
	layer_destroy(spoken_lang_slider_layer);
	layer_destroy(translated_lang_slider_layer);
	layer_destroy(microphone_slider_layer);
	layer_destroy(loading_layer);
	layer_destroy(results_layer);
	
	menu_layer_destroy(spoken_lang_menu_layer);
	menu_layer_destroy(translated_lang_menu_layer);
	
	gbitmap_destroy(mic);
	gbitmap_destroy(restart);
	gbitmap_destroy(quit);
	gbitmap_destroy(arrow);
	
	window_destroy(input_window);
	window_destroy(translation_window);
  window_destroy(results_window);
}

int main(){
	init();
	app_event_loop();
	deinit();
}