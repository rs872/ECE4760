static PT_THREAD (protothread_anim(struct pt *pt))
{
    PT_BEGIN(pt);
    
      while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(32);
        
        int i;
        for (i = 0; i < boids; i++){
            // erase disk
            tft_fillCircle(Accum2int(x_pos[i]), Accum2int(y_pos[i]), 4, ILI9340_BLACK); //x, y, radius, color
            _Accum xpos_avg = 0;
            _Accum ypos_avg = 0;
            _Accum xvel_avg = 0;
            _Accum yvel_avg = 0;
            _Accum neighboring_boids = 0;
            _Accum close_dx = 0;
            _Accum close_dy = 0;
            
            for (int j = 0; j < boids; j++){
                if ( j != i ){
                    
                    //Distance between boids
                    _Accum dx = x_pos[i] - x_pos[j];
                    _Accum dy = y_pos[i] - y_pos[j];
                    
                    //Is it in visual range
                    if (abs(dx) < visual_range && abs(dy) < visual_range){
                        _Accum squared_distance = dx*dx + dy*dy;
                        
                        
                        //Is squared distance less than the protected range?
                        if (squared_distance < protected_range*protected_range){
                            //If so, calculate difference in x/y-coordinates to nearfield boid
                            close_dx += x_pos[i] - x_pos[j]; 
                            close_dy += y_pos[i] - y_pos[j];  
                        }
                    
                        //If not in protected range, is the boid in the visual range?
                        else if (squared_distance < visual_range * visual_range){

                        //Add other boid's x/y-coord and x/y vel to accumulator variables
                        xpos_avg += x_pos[j];
                        ypos_avg += y_pos[j];
                        xvel_avg += x_vel[j];
                        yvel_avg += y_vel[j];

                        //Increment number of boids within visual range
                        neighboring_boids += 1;
                        }
                        

                    //    # If there were any boids in the visual range . . .            
                    //    if (neighboring_boids > 0): 
                    //
                    //        # Divide accumulator variables by number of boids in visual range
                    //        xpos_avg = xpos_avg/neighboring_boids 
                    //        ypos_avg = ypos_avg/neighboring_boids
                    //        xvel_avg = xvel_avg/neighboring_boids
                    //        yvel_avg = yvel_avg/neighboring_boids
                    //
                    //        # Add the centering/matching contributions to velocity
                    //        boid.vx = (boid.vx + 
                    //                   (xpos_avg - boid.x)*centering_factor + 
                    //                   (xvel_avg - boid.vx)*matching_factor)
                    //
                    //        boid.vy = (boid.vy + 
                    //                   (ypos_avg - boid.y)*centering_factor + 
                    //                   (yvel_avg - boid.vy)*matching_factor)
                    //
                    //    # Add the avoidance contribution to velocity
                    //    boid.vx = boid.vx + (close_dx*avoidfactor)
                    //    boid.vy = boid.vy + (close_dy*avoidfactor)
                    }
                }
            }
            //If the boid is near an edge, make it turn by turnfactor
            if (y_pos[i] < top_margin){ //Top left corner is (0,0); at top edge, y_pos is min
                y_vel[i] = y_vel[i] + turn_factor;
            }

            if (x_pos[i] > tft_x - right_margin){ //At right edge, x_pos is max
                x_vel[i] = x_vel[i] - turn_factor;
            }         

            if (x_pos[i] < left_margin){ //at left edge, x_pos is min
                x_vel[i] = x_vel[i] + turn_factor;
            }

            if (y_pos[i] > tft_y - bottom_margin){ //At bottom edge, y_pos is max
                y_vel[i] = y_vel[i] - turn_factor;
            }                     


            // Calculate the boid's speed
            // Slow step! Lookup the "alpha max plus beta min" algorithm

            _Accum speed = sqrt(boid.vx*boid.vx + boid.vy*boid.vy);

            //Enforce min and max speeds
            if (speed < min_speed) {
                x_vel[i] = (x_vel[i]/speed)*min_speed;
                y_vel[i] = (y_vel[i]/speed)*min_speed;
            }
            if (speed > max_speed){ 
                x_vel[i] = (x_vel[i]/speed)*max_speed;
                y_vel[i] = (y_vel[i]/speed)*max_speed;
            }

            //Update boid's position
            x_pos[i] = x_pos[i] + x_vel[i];
            y_pos[i] = y_pos[i] + y_vel[i];
        
        
        //  draw disk
        tft_fillCircle(Accum2int(x_pos[i]), Accum2int(y_pos[i]), 4, ILI9340_WHITE); //x, y, radius, color
      }
        // NEVER exit while
         
      } // END WHILE(1)
  PT_END(pt);
} // animation thread
