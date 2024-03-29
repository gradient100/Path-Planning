#include <fstream>
#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <math.h>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "json.hpp"
#include "spline.h"

using namespace std;

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_first_of("}");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

double distance(double x1, double y1, double x2, double y2)
{
	return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
}

double speed(double vx, double vy)
{
	return sqrt(vx*vx+vy*vy);
}
int ClosestWaypoint(double x, double y, const vector<double> &maps_x, const vector<double> &maps_y)
{

	double closestLen = 100000; //large number
	int closestWaypoint = 0;

	for(int i = 0; i < maps_x.size(); i++)
	{
		double map_x = maps_x[i];
		double map_y = maps_y[i];
		double dist = distance(x,y,map_x,map_y);
		if(dist < closestLen)
		{
			closestLen = dist;
			closestWaypoint = i;
		}

	}

	return closestWaypoint;

}

int NextWaypoint(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{

	int closestWaypoint = ClosestWaypoint(x,y,maps_x,maps_y);

	double map_x = maps_x[closestWaypoint];
	double map_y = maps_y[closestWaypoint];

	double heading = atan2((map_y-y),(map_x-x));

	double angle = fabs(theta-heading);
  angle = min(2*pi() - angle, angle);

  if(angle > pi()/4)
  {
    closestWaypoint++;
  if (closestWaypoint == maps_x.size())
  {
    closestWaypoint = 0;
  }
  }

  return closestWaypoint;
}

// Transform from Cartesian x,y coordinates to Frenet s,d coordinates
vector<double> getFrenet(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int next_wp = NextWaypoint(x,y, theta, maps_x,maps_y);

	int prev_wp;
	prev_wp = next_wp-1;
	if(next_wp == 0)
	{
		prev_wp  = maps_x.size()-1;
	}

	double n_x = maps_x[next_wp]-maps_x[prev_wp];
	double n_y = maps_y[next_wp]-maps_y[prev_wp];
	double x_x = x - maps_x[prev_wp];
	double x_y = y - maps_y[prev_wp];

	// find the projection of x onto n
	double proj_norm = (x_x*n_x+x_y*n_y)/(n_x*n_x+n_y*n_y);
	double proj_x = proj_norm*n_x;
	double proj_y = proj_norm*n_y;

	double frenet_d = distance(x_x,x_y,proj_x,proj_y);

	//see if d value is positive or negative by comparing it to a center point

	double center_x = 1000-maps_x[prev_wp];
	double center_y = 2000-maps_y[prev_wp];
	double centerToPos = distance(center_x,center_y,x_x,x_y);
	double centerToRef = distance(center_x,center_y,proj_x,proj_y);

	if(centerToPos <= centerToRef)
	{
		frenet_d *= -1;
	}

	// calculate s value
	double frenet_s = 0;
	for(int i = 0; i < prev_wp; i++)
	{
		frenet_s += distance(maps_x[i],maps_y[i],maps_x[i+1],maps_y[i+1]);
	}

	frenet_s += distance(0,0,proj_x,proj_y);

	return {frenet_s,frenet_d};

}

// Transform from Frenet s,d coordinates to Cartesian x,y
vector<double> getXY(double s, double d, const vector<double> &maps_s, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int prev_wp = -1;

	while(s > maps_s[prev_wp+1] && (prev_wp < (int)(maps_s.size()-1) ))
	{
		prev_wp++;
	}

	int wp2 = (prev_wp+1)%maps_x.size();

	double heading = atan2((maps_y[wp2]-maps_y[prev_wp]),(maps_x[wp2]-maps_x[prev_wp]));
	// the x,y,s along the segment
	double seg_s = (s-maps_s[prev_wp]);

	double seg_x = maps_x[prev_wp]+seg_s*cos(heading);
	double seg_y = maps_y[prev_wp]+seg_s*sin(heading);

	double perp_heading = heading-pi()/2;

	double x = seg_x + d*cos(perp_heading);
	double y = seg_y + d*sin(perp_heading);

	return {x,y};

}

vector <double> map2car(double map_x, double map_y, double ref_x, double ref_y, double theta)
{
	double car_x = (map_x-ref_x)*cos(0-theta) - (map_y-ref_y)*sin(0-theta);
	double car_y = (map_x-ref_x)*sin(0-theta) + (map_y-ref_y)*cos(0-theta);
	return {car_x, car_y};
}

vector <double> car2map(double car_x, double car_y, double ref_x, double ref_y, double theta)
{
	double map_x = car_x*cos(theta) - car_y*sin(theta) + ref_x;
	double map_y = car_x*sin(theta) + car_y*cos(theta) + ref_y;
	return {map_x, map_y};
}


int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  ifstream in_map_(map_file_.c_str(), ifstream::in);

  string line;
  while (getline(in_map_, line)) {
  	istringstream iss(line);
  	double x;
  	double y;
  	float s;
  	float d_x;
  	float d_y;
  	iss >> x;
  	iss >> y;
  	iss >> s;
  	iss >> d_x;
  	iss >> d_y;
  	map_waypoints_x.push_back(x);
  	map_waypoints_y.push_back(y);
  	map_waypoints_s.push_back(s);
  	map_waypoints_dx.push_back(d_x);
  	map_waypoints_dy.push_back(d_y);
  }

  double ref_speed = 0; // in mph

  int lane = 1;

  h.onMessage([&ref_speed, &lane, &map_waypoints_x,&map_waypoints_y,&map_waypoints_s,&map_waypoints_dx,&map_waypoints_dy](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    //auto sdata = string(data).substr(0, length);
    //cout << sdata << endl;
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          
        	// Main car's localization Data
          	double car_x = j[1]["x"];
          	double car_y = j[1]["y"];
          	double car_s = j[1]["s"];
          	double car_d = j[1]["d"];
          	double car_yaw = j[1]["yaw"];
          	double car_speed = j[1]["speed"];

          	// Previous path data given to the Planner
          	auto prev_path_x = j[1]["previous_path_x"];
          	auto prev_path_y = j[1]["previous_path_y"];
          	// Previous path's end s and d values 
          	double end_path_s = j[1]["end_path_s"];
          	double end_path_d = j[1]["end_path_d"];

          	// Sensor Fusion Data, a list of all other cars on the same side of the road.
          	auto sensor_fusion = j[1]["sensor_fusion"];

          	json msgJson;          	

          	int prev_size = prev_path_x.size();
          	double s_future;
          	if (prev_size > 0)
          		s_future = end_path_s;
          	else s_future = car_s;	
          	
          	bool too_close = false;
          	int lane_right = lane+1;
          	int lane_left = lane-1;
          	bool lane_clear_right = true;
          	bool lane_clear_left = true;
          	double next_s_right=10000; // speed of the leading car in right lane
          	double next_s_left=10000; // speed of the leading carin left lane
          	double left_speed = -1;
          	double right_speed = -1;
          	bool is_turning = false;
          	double max_speed = 39.5; // in mph
          	double max_turn_speed = 20;
          	double too_close_distance = 0;

          	for (int i = 0; i < sensor_fusion.size(); i++)
          	{
          		double other_d = sensor_fusion[i][6];
          		int other_lane = other_d / 4;
          		double other_speed = 2.24*speed(sensor_fusion[i][3], sensor_fusion[i][4]); // in mph
      			double other_s = sensor_fusion[i][5];
      			double other_s_future = other_s + 0.02*other_speed/2.24*prev_size;
      			
          		if (other_lane == lane) // see how close car in front in same lane is
          		{
          			// if ( (other_s_future-s_future) > 0 && (other_s_future-s_future < 50) )
          			//too_close_distance = other_s - car_s;
          			if (0 < other_s-car_s && other_s-car_s < 40 )	
          			{
          				too_close = true;
						too_close_distance = other_s - car_s;	

          			}
          				
          		}

          		if (other_lane == lane_left) // see if there are any cars in left change lane area
          		{
          			if (car_s-30 < other_s && other_s < car_s+40)
  					//if (s_future-10 < other_s_future && other_s < car_s+40)
  						lane_clear_left = false;
  					if (other_s > car_s+40 && other_s < next_s_left) // record speed of left leading car
	  				{
	  					next_s_left = other_s;
	  					left_speed = other_speed; // in mph
	  				}
  				}
  				if (other_lane == lane_right) // see if there are any cars in right change lane
          		{
          			if (car_s-30 < other_s && other_s < car_s+40)
          			//if (s_future-10 < other_s_future && other_s < car_s+40)
  						lane_clear_left = false;
  						lane_clear_right = false;
  					if (other_s > car_s+40 && other_s < next_s_right) // record speed of right leading car
	  				{
	  					next_s_right = other_s;
	  					right_speed = other_speed; // in mph
	  				}
  				}
	  			
  			/*	cout << "Other lane: " << other_lane 
  					 << " Distance from car " << other_s - car_s
  					 << " Other speed : " << other_speed << endl;
   			*/
          	}
          	//if (too_close || ( (ref_speed < max_speed) && ref_speed>0) ) 
          	if (too_close)
          	{
           		
           		if (!lane_clear_right)
           			right_speed = -1;

           		if (!lane_clear_left)
           			left_speed = -1;
           	/*
           		cout << "**********************************" << endl;
           		cout << "Too close!"<<endl;
           		cout << "lane clear left : " << lane_clear_left << endl;
           		cout << "lane clear right : " << lane_clear_right << endl;
           		cout << "lane speed left : " << left_speed << endl;
           		cout << "lane speed right : " << right_speed << endl;
           		cout << " *********************************" << endl;
           	*/	
	      		// left_speed = getSpeed("left", lane, car_s, sensor_fusion)
	      		// right_speed = getSpeed("right", lane, car_s, sensor_fusion)	
           		
          		// can change if the best speed after changing lanes is the best compared 
          		// to keeping lanes or changing to the other lane
          		if (left_speed > ref_speed && left_speed > right_speed)
          		{
          	 		lane--;
          	 		//is_turning = true;
          		}
          	 	else if (right_speed > ref_speed && right_speed > left_speed)
          	 	{
          	 		lane++;
          	 		//is_turning = true;
          	 	}

          	 	// if can't change lanes and too close to front car in same lane, slow down based on 
          	 	// distance from front car
          	 	else if (too_close)
          	 	{
          	 		//ref_speed -= 0.224;
          	 		//ref_speed -= (0.4 - 0.01*too_close_distance);
          	 		if (too_close_distance <= 10)
          	 			ref_speed -= 0.44;
          	 		else if (too_close_distance <= 20)
          	 			ref_speed -= 0.224;
          	 		else if (too_close_distance <= 40)
          	 			ref_speed -= 0.224;

          	 		//cout << "Too close distance = " << too_close_distance
          	 		//	 << "\t speed : " << ref_speed <<endl;
          	 	}
          	 		

           }
           // speed up, choosing speed not exceeding jerk
           if (!too_close && ref_speed < max_speed)
	      			ref_speed += 0.18; // 0.224 mph over 0.02s results in acceleration of 5 m/s^2
	      			//ref_speed += 0.224; // 0.224 mph over 0.02s results in acceleration of 5 m/s^2

	        // else if (is_turning)
	        //  	if (ref_speed > max_turn_speed)
	        //   			ref_speed -= 0.4;
	        //   	else if (ref_speed < max_speed)
	        //   			ref_speed += 0.18;

	          		

         	
          	// load in points for spline fitting
          	vector<double> spline_x;
          	vector<double> spline_y;
          	double prev_x, penult_x;
          	double prev_y, penult_y;
          	double ref_x = car_x;
          	double ref_y = car_y;
          	double ref_yaw = deg2rad(car_yaw);


          	if (prev_size < 2)
          	{
          		ref_x = car_x;
          		prev_x = car_x; 

          		ref_y = car_y;
          		prev_y = car_y;
          		
          		
          		ref_yaw = deg2rad(car_yaw);
          		penult_x = prev_x - cos(ref_yaw);
          		penult_y = prev_y - sin(ref_yaw);
          		
          	}
          	else 
          	{
          		prev_x = prev_path_x[prev_size-1];
          		ref_x = prev_x;

          		prev_y = prev_path_y[prev_size-1];
          		ref_y = prev_y;

          		penult_x = prev_path_x[prev_size-2];
          		penult_y = prev_path_y[prev_size-2];
          		ref_yaw = atan2( (prev_y-penult_y), (prev_x-penult_x) );	
          	}	
          	spline_x.push_back(penult_x);
          	spline_x.push_back(prev_x);
          	spline_y.push_back(penult_y);
          	spline_y.push_back(prev_y);

          	// load future path points to fit spline
          	spline_x.push_back(getXY(car_s+40, lane*4+2, map_waypoints_s, map_waypoints_x, map_waypoints_y)[0]);
          	spline_x.push_back(getXY(car_s+80, lane*4+2, map_waypoints_s, map_waypoints_x, map_waypoints_y)[0]);
          	spline_x.push_back(getXY(car_s+120, lane*4+2, map_waypoints_s, map_waypoints_x, map_waypoints_y)[0]);
          	spline_y.push_back(getXY(car_s+40, lane*4+2, map_waypoints_s, map_waypoints_x, map_waypoints_y)[1]);
          	spline_y.push_back(getXY(car_s+80, lane*4+2, map_waypoints_s, map_waypoints_x, map_waypoints_y)[1]);
          	spline_y.push_back(getXY(car_s+120, lane*4+2, map_waypoints_s, map_waypoints_x, map_waypoints_y)[1]);

          	for (int i=0; i < spline_x.size(); i++)
          	{
          		vector <double> spline_local = map2car(spline_x[i], spline_y[i], ref_x, ref_y, ref_yaw);
          		spline_x[i] = spline_local[0];
          		spline_y[i] = spline_local[1];
          	}

          	tk::spline s;
          	s.set_points(spline_x, spline_y);

          	vector<double> next_x_vals;
          	vector<double> next_y_vals;
          	for (int i=0; i < prev_size; i++)
          	{
          		next_x_vals.push_back(prev_path_x[i]);
          		next_y_vals.push_back(prev_path_y[i]);
          	}		

          	// load spline points spaced the distance that allows current speed
          	double target_x = 40; 
          	double target_y = s(target_x);
          	double target_d = sqrt(target_x*target_x + target_y*target_y);
          	double next_x = 0;
          	for (int i=0; i < 50-prev_size; i++)
          	{
          		double N = target_d / (0.02* ref_speed / 2.24);

          		next_x += target_x/N;
          		vector<double> map_xy = car2map(next_x, s(next_x), ref_x, ref_y, ref_yaw);
          		next_x_vals.push_back(map_xy[0]);
          		next_y_vals.push_back(map_xy[1]);


          	}	
          	

		// end calculation of next x,y points

          	msgJson["next_x"] = next_x_vals;
          	msgJson["next_y"] = next_y_vals;

          	auto msg = "42[\"control\","+ msgJson.dump()+"]";

          	//this_thread::sleep_for(chrono::milliseconds(1000));
          	ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
          
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the
  // program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
