#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#define MY_UUID { 0x25, 0x3C, 0x55, 0x83, 0x38, 0x32, 0x47, 0xCB, 0x9D, 0xC0, 0x45, 0x8D, 0x12, 0x05, 0xE9, 0xEF }
PBL_APP_INFO(MY_UUID,
	     "SteamPebble Mark I", "Glenn Loos-Austin",
	     1, 0, /* App major/minor version */
	     RESOURCE_ID_IMAGE_MENU_ICON,
	     APP_INFO_WATCH_FACE);

//Major props to Pebble Technology & orviwan's 91dub, for providing the base of the code used, thus allowing me to
//concentrate on the design elements needed for my cunning plans.

Window window;
Layer timeFrame; //this is necessary to frame the digits so that they can be animated with the property animation tool yet be clipped when they move down
                    // outside of the watch frame.

GRect from_rect[4];  //to restore digits to their starting positions properly.

bool isDown[] = {true,true,true,true}; //should start in "down" state, so they can animate up during load.
bool minuteAnimating = false; //state of rapid animation of gears during the 3 second time animations.

int count_down_to = -1; // hmm, this is used in handle_second_tick, to determine how many digits need to be animated.
        // if I revise the code, I'll probably move it into that routine, I don't think it needs to be a global anymore.

int _gearCounter = 1; //this determines what frame of the gear animation we're on. It probably needs an upper bounds check somewhere, since in theory it increases indefinitely.

AppTimerHandle timer_handle; //to hold the animation timer for the gears updating 10 times per second.

BmpContainer background_image;

// BmpContainer meter_bar_image;    //from 91dub, currently not doing this.
// BmpContainer time_format_image;  //from 91dub, currently not doing this.


const int DAY_NAME_IMAGE_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_DAY_NAME_SUN,
  RESOURCE_ID_IMAGE_DAY_NAME_MON,
  RESOURCE_ID_IMAGE_DAY_NAME_TUE,
  RESOURCE_ID_IMAGE_DAY_NAME_WED,
  RESOURCE_ID_IMAGE_DAY_NAME_THU,
  RESOURCE_ID_IMAGE_DAY_NAME_FRI,
  RESOURCE_ID_IMAGE_DAY_NAME_SAT
};

BmpContainer day_name_image;


const int DATENUM_IMAGE_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_DATENUM_0,
  RESOURCE_ID_IMAGE_DATENUM_1,
  RESOURCE_ID_IMAGE_DATENUM_2,
  RESOURCE_ID_IMAGE_DATENUM_3,
  RESOURCE_ID_IMAGE_DATENUM_4,
  RESOURCE_ID_IMAGE_DATENUM_5,
  RESOURCE_ID_IMAGE_DATENUM_6,
  RESOURCE_ID_IMAGE_DATENUM_7,
  RESOURCE_ID_IMAGE_DATENUM_8,
  RESOURCE_ID_IMAGE_DATENUM_9
};


#define TOTAL_DATE_DIGITS 2
BmpContainer date_digits_images[TOTAL_DATE_DIGITS];


const int BIG_DIGIT_IMAGE_RESOURCE_IDS[] = {
    RESOURCE_ID_IMAGE_NUM_0,
    RESOURCE_ID_IMAGE_NUM_1,
    RESOURCE_ID_IMAGE_NUM_2,
    RESOURCE_ID_IMAGE_NUM_3,
    RESOURCE_ID_IMAGE_NUM_4,
    RESOURCE_ID_IMAGE_NUM_5,
    RESOURCE_ID_IMAGE_NUM_6,
    RESOURCE_ID_IMAGE_NUM_7,
    RESOURCE_ID_IMAGE_NUM_8,
    RESOURCE_ID_IMAGE_NUM_9
};

#define TOTAL_TIME_DIGITS 4
BmpContainer time_digits_images[TOTAL_TIME_DIGITS];
PropertyAnimation digit_animations[TOTAL_TIME_DIGITS];  //4 animations, 1 per digit, since they update at different rates.

const int GEAR_IMAGE_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_GEAR_0,
    RESOURCE_ID_IMAGE_GEAR_1,
    RESOURCE_ID_IMAGE_GEAR_2,
    RESOURCE_ID_IMAGE_GEAR_3,
    RESOURCE_ID_IMAGE_GEAR_4,
    RESOURCE_ID_IMAGE_GEAR_5,
    RESOURCE_ID_IMAGE_GEAR_6,
    RESOURCE_ID_IMAGE_GEAR_7,
    RESOURCE_ID_IMAGE_GEAR_8,
    RESOURCE_ID_IMAGE_GEAR_9,
    RESOURCE_ID_IMAGE_GEAR_10,
    RESOURCE_ID_IMAGE_GEAR_11,
    RESOURCE_ID_IMAGE_GEAR_12,
    RESOURCE_ID_IMAGE_GEAR_13,
    RESOURCE_ID_IMAGE_GEAR_14
};
BmpContainer gear_image;

//
// Main image setting routine.
// Used for every image swap in the app, the gears, the time digits, the day of week, the date
//

void set_container_image(BmpContainer *bmp_container, const int resource_id, GPoint origin, Layer *targetLayer) {

  layer_remove_from_parent(&bmp_container->layer.layer);            //remove it from layer so it can be safely deinited
  bmp_deinit_container(bmp_container);                              //deinit the old image.

  bmp_init_container(resource_id, bmp_container);                   //init the container with the new image

  GRect frame = layer_get_frame(&bmp_container->layer.layer);       //posiiton the new image with the supplied coordinates.
  frame.origin.x = origin.x;
  frame.origin.y = origin.y;
  layer_set_frame(&bmp_container->layer.layer, frame);

  layer_add_child(targetLayer, &bmp_container->layer.layer);        //add the new image to the target layer.
}


//
// Get Display Hour.
// turn the display hour into the digits we actually want to display.
//

unsigned short get_display_hour(unsigned short hour) {

  if (clock_is_24h_style()) {
    return hour;
  }

  unsigned short display_hour = hour % 12;

  // Converts "0" to "12"
  return display_hour ? display_hour : 12;

}

//
// Update Display
// This, called at the beginning, then updated once a minute, changes the display elements into their new images.
// Some weird provision needed to be made for the main clock digits, which have two different possible positions depending on
// whether or not they are currently up or down. (inline conditions on the isDown[] boolean array are used to determine this.
//

void update_display(PblTm *current_time) {
  // TODO: Only update changed values?

  set_container_image(&day_name_image, DAY_NAME_IMAGE_RESOURCE_IDS[current_time->tm_wday], GPoint(65, 64), &window.layer);

  // TODO: Remove leading zero?
  set_container_image(&date_digits_images[0], DATENUM_IMAGE_RESOURCE_IDS[current_time->tm_mday/10], GPoint(101, 64), &window.layer);
  set_container_image(&date_digits_images[1], DATENUM_IMAGE_RESOURCE_IDS[current_time->tm_mday%10], GPoint(109, 64), &window.layer);


  unsigned short display_hour = get_display_hour(current_time->tm_hour);

  // TODO: Remove leading zero?
    set_container_image(&time_digits_images[0], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour/10], isDown[0] ? GPoint(2, 50) : GPoint(2,0), &timeFrame);
  set_container_image(&time_digits_images[1], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour%10], isDown[1] ? GPoint(30, 50) : GPoint(30,0), &timeFrame);

  set_container_image(&time_digits_images[2], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_min/10], isDown[2] ? GPoint(67, 50) : GPoint(67,0), &timeFrame);
  set_container_image(&time_digits_images[3], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_min%10], isDown[3] ? GPoint(95, 50) : GPoint(95,0), &timeFrame); 

  if (!clock_is_24h_style()) {
   // if (current_time->tm_hour >= 12) {
      //set_container_image(&time_format_image, RESOURCE_ID_IMAGE_PM_MODE, GPoint(17, 68));
   // } else {
      //layer_remove_from_parent(&time_format_image.layer.layer);
      //bmp_deinit_container(&time_format_image);
   // }

    if (display_hour/10 == 0) {
      layer_remove_from_parent(&time_digits_images[0].layer.layer);
      bmp_deinit_container(&time_digits_images[0]);
    }
  }

}

//
// Minute updates
// Does very little. Called once a minute by handle_second_tick, calls update_display with the current time.
//

void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) {
  (void)ctx;

  update_display(t->tick_time);
}


//
// Handle timer
// Animates the rapid motion of the gears. (when it's just one gear movement per second, that's handled in handle_second_tick)
//

#define COOKIE_MY_TIMER 1

void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie) {
    (void)ctx;
    (void)handle;
        
    if (cookie == COOKIE_MY_TIMER) {
        //update the gear to the next frame of animation.
        _gearCounter++;
        set_container_image(&gear_image, GEAR_IMAGE_RESOURCE_IDS[_gearCounter%15], GPoint(9, 9), &window.layer);
    }
    
    if(_gearCounter < 300 || minuteAnimating) {
        //if we're in the first 30 seconds, keep the animation moving fast OR
        //if it's during the animation of the minute digits, keep the animation moving fast.
        timer_handle = app_timer_send_event(ctx, 100 /* milliseconds */, COOKIE_MY_TIMER);
    }
}

//
// Handle Second Tick
// The main update happens here. Called once a second.
// 

void handle_second_tick(AppContextRef ctx, PebbleTickEvent *t) {
    (void)ctx;
    
    unsigned short display_second = t->tick_time->tm_sec;
    
    //  bmp_init_container(RESOURCE_ID_IMAGE_METER_BAR, &meter_bar_image);
    
    // meter_bar_image.layer.layer.frame.origin.x = 9+(display_second*2);  // move the meter bar as a second hand. 
    // layer_set_hidden(&(meter_bar_image.layer.layer),false);
    // layer_mark_dirty(&(meter_bar_image.layer.layer));
    
    //set_container_image(&meter_bar_image,RESOURCE_ID_IMAGE_METER_BAR,GPoint(77-display_second,43));
    
    if(_gearCounter > 299 || !minuteAnimating) {
        //if we're not doing a rapid gear animation, we should still update once per second.
        //removing this would save some battery life, I imagine.
        _gearCounter++;
        set_container_image(&gear_image, GEAR_IMAGE_RESOURCE_IDS[_gearCounter%15], GPoint(9, 9), &window.layer);
    }
  
    if(display_second==58)
    {
        unsigned short display_hour = get_display_hour(t->tick_time->tm_hour);
        
        //figure out how many digits will be updating in 2 seconds.
        count_down_to = 3;
        if (t->tick_time->tm_min%10 == 9)
        {
            count_down_to = 2;
            if (t->tick_time->tm_min/10 == 5)
            {
                count_down_to = 1;
                if (display_hour==9 || display_hour==19 || display_hour==23)
                {
                    count_down_to = 0;
                }
                if (display_hour==12 && !clock_is_24h_style())
                {
                    count_down_to = 0;
                }
            }
        }
        
        //in 2 seconds, at least one digit will be changing. We animate all the digits that will be updating off of the bottom thier layer's frame.
        animation_unschedule_all(); //just in case. This isn't likely to occur, but could happen if the app is launched near minute transition
        minuteAnimating= true; //spin the wheels while moving the digitis.
  
        if (_gearCounter>299)
        {
            //we're not doing the initial quick animation anymore, so we have to start our own.
            timer_handle = app_timer_send_event(ctx, 100 /* milliseconds */, COOKIE_MY_TIMER);
        }
        
        for(int i=3;i>=count_down_to;i--)
        {
            //for each digit that's going to be changing.
            isDown[i] = true;  //mark it as down so that updateImage knows to redraw new digit outside the layer frame.
            
            GRect to_rect = GRect(0, 0, 0, 0);
            to_rect = from_rect[i]; //what's its base position?
            to_rect.origin.y += 50; //we want to move it down 50 pixels, to get it outside the frame.
            
            //set up and start the animation.
            property_animation_init_layer_frame(&digit_animations[i], &time_digits_images[i].layer.layer, NULL, &to_rect);
            animation_set_duration(&digit_animations[i].animation, 1750-(250*i));
            animation_set_curve(&digit_animations[i].animation,AnimationCurveEaseIn);
            animation_schedule(&digit_animations[i].animation);
        }
        
    }
    if(display_second==0) {
        
        handle_minute_tick(ctx,t);  //we call this rather than having the OS do so, so we can control exactly when it's going to happen.
        animation_unschedule_all(); //just in case. This isn't likely to occur, but could happen if the app is launched near minute transition.
        
        //animate the digits back to starting positions!
        for(int i=3;i>=0;i--)
        {
            if(isDown[i]) {
                //if we put it down, so set up and start the animation to get it back up.
                property_animation_init_layer_frame(&digit_animations[i], &time_digits_images[i].layer.layer, NULL, &from_rect[i]);
                animation_set_duration(&digit_animations[i].animation, 1250-(125*i));
                animation_set_curve(&digit_animations[i].animation,AnimationCurveEaseIn);
                animation_schedule(&digit_animations[i].animation);
                isDown[i] = false;
            }
        }
    }
    if(display_second==1) {
        //stop the gears spinning fast, and go back to 1 update per second.
        minuteAnimating = false;
    }
        
} //end handle_second_tick



void handle_init(AppContextRef ctx) {
  (void)ctx;
    

  window_init(&window, "91 Gears");
  window_stack_push(&window, true /* Animated */);

  resource_init_current_app(&APP_RESOURCES);

  bmp_init_container(RESOURCE_ID_IMAGE_BACKGROUND, &background_image);
  layer_add_child(&window.layer, &background_image.layer.layer);
    
    layer_init(&timeFrame, GRect(9, 82, 125, 50)); //clipping region for big digits.
    layer_add_child(&window.layer, &timeFrame); //clipping region for time numbers

    
 // bmp_init_container(RESOURCE_ID_IMAGE_METER_BAR, &meter_bar_image);

 // meter_bar_image.layer.layer.frame.origin.x = 17;
 // meter_bar_image.layer.layer.frame.origin.y = 43;
 // layer_set_hidden(&(meter_bar_image.layer.layer),true);

 // layer_add_child(&window.layer, &meter_bar_image.layer.layer);


 // if (clock_is_24h_style()) {
  //  bmp_init_container(RESOURCE_ID_IMAGE_24_HOUR_MODE, &time_format_image);

 //   time_format_image.layer.layer.frame.origin.x = 17;
  //  time_format_image.layer.layer.frame.origin.y = 68;

  //  layer_add_child(&window.layer, &time_format_image.layer.layer);
  // }


  // Avoids a blank screen on watch start.
  PblTm tick_time;

  get_time(&tick_time);
    
  update_display(&tick_time);

    //start by animating 3 or 4 digits up from the bottom of the display, slower than we do later on for dramatic effect.
    for(int i=3;i>=0;i--)
    {
        from_rect[i] = layer_get_frame(&time_digits_images[i].layer.layer);
        from_rect[i].origin.y-=50;
        property_animation_init_layer_frame(&digit_animations[i], &time_digits_images[i].layer.layer, NULL, &from_rect[i]);
        animation_set_duration(&digit_animations[i].animation, 2500-(400*i));
        animation_set_curve(&digit_animations[i].animation,AnimationCurveEaseIn);
        animation_schedule(&digit_animations[i].animation);
        isDown[i] = false;
    }
    
    timer_handle = app_timer_send_event(ctx, 100 /* milliseconds */, COOKIE_MY_TIMER);

} //end handle_init


void handle_deinit(AppContextRef ctx) {
  (void)ctx;

  bmp_deinit_container(&background_image);
  //bmp_deinit_container(&meter_bar_image);
  //bmp_deinit_container(&time_format_image);
  bmp_deinit_container(&day_name_image);
  bmp_deinit_container(&gear_image);
    
  for (int i = 0; i < TOTAL_DATE_DIGITS; i++) {
    bmp_deinit_container(&date_digits_images[i]);
  }

  for (int i = 0; i < TOTAL_TIME_DIGITS; i++) {
    bmp_deinit_container(&time_digits_images[i]);
  }
    
}


void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
      .timer_handler = &handle_timer,

    .tick_info = {
      .tick_handler = &handle_second_tick,
      .tick_units = SECOND_UNIT
    }
  };
  app_event_loop(params, &handlers);
}
