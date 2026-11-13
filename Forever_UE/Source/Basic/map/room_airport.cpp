#include "room_airport.h"

using namespace std;

int AirportRoom::count = 0;

AirportRoom::AirportRoom() : id(count++) {
}

const char* AirportRoom::GetName() {
	name = "AirportRoom" + to_string(id);
	return name.data();
}

void AirportRoom::SetProperty() {
	// 房间沿正面墙(u轴)的两侧各一个停机位。用房间局部坐标系(见ParkingSpot注释)，房间按朝向旋转时车位跟着转：
	// 第一对是u轴，ratio相对房间角点，0/1正好是房间沿正面墙的两端边缘，-0.15/1.15落在房间外侧
	// (估计值，后面按实际机翼宽度调)；第二对是v轴，0.5=和房间同一条中线。
	// rotationDegrees=0表示朝向房间正面(朝路)；跑道在房间深处(AirStation的depthRatio 0.65/0.85)，
	// 所以停机位朝向跑道是180度。
	constexpr float kLeftAlong = -0.15f;
	constexpr float kRightAlong = 1.15f;
	constexpr float kExtraOffset = 0.5f; // 再往两侧挪的绝对距离(地图单位，1单位=10米)，不受房间尺寸影响
	isParking = true;
	parkingSpots = {
		{ {kLeftAlong, -kExtraOffset, 0.5f, 0.f}, 180.f, { "plane" } },
		{ {kRightAlong, kExtraOffset, 0.5f, 0.f}, 180.f, { "plane" } },
	};
}
