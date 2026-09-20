#include "route_basic.h"

using namespace std;

int RouteBasic::count = 0;

RouteBasic::RouteBasic() : id(count++) {
}

const char* RouteBasic::GetName() {
	name = "RouteBasic" + to_string(id);
	return name.data();
}
