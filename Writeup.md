# CarND-Path-Planning-Project
Self-Driving Car Engineer Nanodegree Program

# Model Documentation

### Overview

The program uses data pertaining to the main car's localization, sensor fusion data for all other cars and a map of waypoints needed to be traversed.  The program calculates the next 50 points in the path trajectory, corresponding to approximately 20 meters into the future.  The points are fitted with a smooth spline in order to avoid sharp points in the path, which lead to discontinuities in time derivatives, affecting acceleration and jerk constraint adherences.  Points are calculated based on whether the car ahead in the same lane is too close, and if so, whether turning to either adjacent lane is possible.

### Path Generation

**Sensor Fusion** 

Sensor Fusion Data is first used to make a decision on whether to keep the current lane (and at what speed) or whether to change lanes.  Sensor fusion keeps track of the state of every other car on the road, with data on the x-coordinate, y-coordinate, s-coordinate, d-coordinate, and velocity.  

For each car from the sensor fusion data, the program identifies whether there is a car ahead in the same lane that is too close, within 40meters.  Also, the program identifies whether the adjacent lanes are clear for lane changing-- in the adjacent lanes, there should be no car behind the main car within 30m or in front of the main car within 40m.  This range was chosen because 30m clearance behind the main car for lane changes is safe in case a car is accelerating in an adjacent lane from behind; also, 40m clearance in front of the main car for lane changes allows a possible lane change that is not too sharp that it violates the jerk constraints.

For lane change information, the program also uses sensor fusion data to find the nearest car ahead in the adjacent lane, in order to make a decision on whether it is optimal to make a lane change.

**Decisions to Change Lanes or Slow Down**

Lane change occurs when the adjacent lane is clear in the range discussed above and also the nearerest car ahead in that lane is travelling faster than in the current lane and faster than the nearest car ahead in an alternate adjacent lane.

Slowing down occurs when the car ahead in the current lane is within 40 meters of the main car and lane change is not possible.

**Spline Fitting**

Once the determination is made to stay lanes or change lanes, a smooth spline is fitted onto the predicted path, in order to avoid sharp points in the path, which lead to discontinuities to time derivatives, affectiving acceleration and jerk.

The spline is made up of the last two points in the previously predicted path, in addition to sparse points spaced 40m, 80m, and 120m ahead.  Before the spline is fitted to the points, the points are first converted from map coordinates to the car's local coordinated, with the x-axis pointing in the same direction as the car's heading (yaw), and the y-axis 90 degrees counterclockwise from the x-axis.

Points from the spline are then added to the previous unused trajectory (for a total of 50 points), each point spaced by the distance the car travels in 0.02 sec with the car's current speed.




