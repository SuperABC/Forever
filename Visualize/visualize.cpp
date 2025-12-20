#include "winsgl.h"

#include "parser.h"
#include "map.h"
#include "populace.h"
#include "society.h"
#include "story.h"
#include "error.h"

#include <iostream>
#include <memory>
#include <algorithm>

#pragma comment(lib, "Forever.lib")
#pragma comment(lib, "Dependence.lib")


using namespace std;

enum DISP_MODE {
	DISP_TERRAIN,
	DISP_PLOT,
	DISP_CHUNK,
	DISP_END
};
int dispMode = 0;

unique_ptr<Map> map = make_unique<Map>();
unique_ptr<Populace> populace = make_unique<Populace>();
unique_ptr<Society> society = make_unique<Society>();
unique_ptr<Story> story = make_unique<Story>();


int sizeX = 640, sizeY = 460;
int steps[] = { 8, 4, 2, 1 };

int cameraX, cameraY;
int zoom = 0;

Building* currentBuilding = NULL;
int buildingWindow = 0;
bool buildingInfo = false;
Zone* currentZone = NULL;
int zoneWindow = 0;
bool zoneInfo = false;

unordered_map<string, rgb> terrainColors;
unordered_map<string, rgb> chunkColors;

void updateBuilding(int floor, int scroll) {
	setColor(255, 255, 255);
	clearScreen();

	auto currentFloor = currentBuilding->GetFloor(floor);
	if (currentFloor) {
		for (auto facility : currentFloor->GetFacilities()) {
			switch (facility.getType()) {
			case Facility::FACILITY_CORRIDOR:
				setColor(127, 127, 127);
				break;
			case Facility::FACILITY_STAIR:
				setColor(0, 255, 0);
				break;
			case Facility::FACILITY_ELEVATOR:
				setColor(0, 255, 255);
				break;
			default:
				break;
			}
			putQuad(currentBuilding->GetConstruction().GetLeft() * 20 + facility.GetLeft() * 20,
				currentBuilding->GetConstruction().GetBottom() * 20 + facility.GetBottom() * 20,
				currentBuilding->GetConstruction().GetLeft() * 20 + facility.GetRight() * 20,
				currentBuilding->GetConstruction().GetBottom() * 20 + facility.GetTop() * 20, SOLID_FILL);
		}
		setColor(0, 0, 0);
		for (auto room : currentBuilding->GetRooms()) {
			if(room->GetLayer() == floor)
			putQuad(currentBuilding->GetConstruction().GetLeft() * 20 + room->GetLeft() * 20,
				currentBuilding->GetConstruction().GetBottom() * 20 + room->GetBottom() * 20,
				currentBuilding->GetConstruction().GetLeft() * 20 + room->GetRight() * 20,
				currentBuilding->GetConstruction().GetBottom() * 20 + room->GetTop() * 20, EMPTY_FILL);
		}
	}

	setColor(0, 0, 0);
	putString((currentBuilding->GetName() + "\n" +
		"宽" + to_string(currentBuilding->GetSizeX() * 10) + "m" + "\n" +
		"高" + to_string(currentBuilding->GetSizeY() * 10) + "m" + "\n" +
		"面积" + to_string(currentBuilding->GetAcreage()) + "m2" + "\n" +
		"地面层高" + to_string(currentBuilding->GetLayers()) + "层" + "\n" +
		"地下层高" + to_string(currentBuilding->GetBasements()) + "层" + "\n" +
		"").data(), currentBuilding->GetSizeX() * 20, 0);
}

void buildingSetup(void* b) {
	currentBuilding = (Building*)b;

	windowFinish([]() {
		currentBuilding = NULL;
		buildingInfo = false;
		});
}

void buildingLoop() {
	static int floor = 0;
	static int scroll = 0;

	if (!buildingInfo) {
		buildingInfo = true;

		updateBuilding(floor, scroll);
	}

	int key;
	if (biosKey(1)) {
		key = biosKey(0);
		switch (key) {
		case 'e':
			floor++;
			break;
		case 'q':
			floor--;
			break;
		}
		floor = clamp(floor, -currentBuilding->GetBasements(), currentBuilding->GetLayers() - 1);

		updateBuilding(floor, scroll);
	}

	vec3i mouse;
	setColor(0, 0, 0);
	if (biosMouse(1).z) {
		mouse = biosMouse(0);
		if (mouse.z == SG_MIDDLE_BUTTON_UP) {
			scroll--;
			scroll = max(0, scroll);
			updateBuilding(floor, scroll);
		}
		if (mouse.z == SG_MIDDLE_BUTTON_DOWN) {
			scroll++;
			updateBuilding(floor, scroll);
		}
	}
}

void updateZone() {
	int off = 0;
	for (auto building : currentZone->GetBuildings()) {
		if (building.second->GetAcreage() == 0)continue;
		srand(off);
		setColor(64 + rand() % 192, 64 + rand() % 192, 64 + rand() % 192);
		putQuad(building.second->GetLeft() * 20, building.second->GetBottom() * 20,
			building.second->GetRight() * 20, building.second->GetTop() * 20, SOLID_FILL);
		setColor(0, 0, 0);
		putString(building.first.data(),
			building.second->GetLeft() * 20,
			building.second->GetBottom() * 20);
		off++;
	}
}

void zoneSetup(void* z) {
	currentZone = (Zone*)z;

	windowFinish([]() {
		currentZone = NULL;
		zoneInfo = false;
		});
}

void zoneLoop() {
	if (!zoneInfo) {
		zoneInfo = true;
		setColor(0, 0, 0);
		putString((currentZone->GetName() + "\n" +
			"宽" + to_string(currentZone->GetSizeX() * 10) + "m" + "\n" +
			"高" + to_string(currentZone->GetSizeY() * 10) + "m" + "\n" +
			"面积" + to_string(currentZone->GetAcreage()) + "m2" + "\n" +
			"").data(), currentZone->GetSizeX() * 20, 0);

		updateZone();
	}

	vec3i mouse;
	if (biosMouse(1).z) {
		mouse = biosMouse(0);
		if (mouse.z == SG_LEFT_BUTTON) {
			int posX = mouse.x / 20 + 1;
			int posY = mouse.y / 20 + 1;
			auto pos = currentZone->GetParent()->GetPosition(currentZone->GetLeft() + posX, currentZone->GetBottom() + posY);
			auto element = ::map->GetElement(pos.first, pos.second);
			if (element && element->GetBuilding().size() > 0) {
				auto building = currentZone->GetBuilding(element->GetBuilding());
				if (building) {
					if (currentBuilding) {
						closeWindow(buildingWindow);
					}
					buildingWindow = createParamWindow(
						round(building->GetSizeX()) * 20 + 200, round(building->GetSizeY()) * 20,
						element->GetBuilding().data(), BIT_MAP, buildingSetup, buildingLoop, building.get());
				}
			}
		}
	}
}

void updateGraph(int x, int y, int zoom, int left = 0, int right = sizeX, int top = 0, int bottom = sizeY) {
	for (int i = top; i < bottom; i++) {
		for (int j = left; j < right; j++) {
			rgb color = { 0, 0, 0 };

			int posX = x + (j - sizeX / 2) / (8 / steps[zoom]);
			int posY = y + (i - sizeY / 2) / (8 / steps[zoom]);

			if (!::map->CheckXY(posX, posY)) {
				setColor(color.r, color.g, color.b);
				putPixel(j, i);
				continue;
			}

			auto terrain = ::map->GetElement(posX, posY)->GetTerrain();
			if (terrainColors.find(terrain) != terrainColors.end()) {
				color = terrainColors[terrain];
			}
			else {
				color = { (unsigned char)GetRandom(128), (unsigned char)GetRandom(128), (unsigned char)GetRandom(128) };
				terrainColors[terrain] = color;
			}

			setColor(color.r, color.g, color.b);
			putPixel(j, i);
		}
	}

	auto roadnet = ::map->GetRoadnet();

	if (dispMode == DISP_PLOT) {
		auto plots = roadnet->GetPlots();
		for (auto plot : plots) {
			auto v1 = plot->GetVertex(1);
			auto v2 = plot->GetVertex(2);
			auto v3 = plot->GetVertex(3);
			auto v4 = plot->GetVertex(4);
			float x1 = sizeX / 2 + (v1.first - cameraX) * (8 / steps[zoom]);
			float y1 = sizeY / 2 + (v1.second - cameraY) * (8 / steps[zoom]);
			float x2 = sizeX / 2 + (v2.first - cameraX) * (8 / steps[zoom]);
			float y2 = sizeY / 2 + (v2.second - cameraY) * (8 / steps[zoom]);
			float x3 = sizeX / 2 + (v3.first - cameraX) * (8 / steps[zoom]);
			float y3 = sizeY / 2 + (v3.second - cameraY) * (8 / steps[zoom]);
			float x4 = sizeX / 2 + (v4.first - cameraX) * (8 / steps[zoom]);
			float y4 = sizeY / 2 + (v4.second - cameraY) * (8 / steps[zoom]);

			setColor(255, 0, 0);
			putTriangle(x1, y1, x2, y2, x3, y3, SOLID_FILL);
			putTriangle(x1, y1, x3, y3, x4, y4, SOLID_FILL);
		}
	}
	else if (dispMode == DISP_CHUNK) {
		auto plots = roadnet->GetPlots();
		for (auto plot : plots) {
			auto zones = plot->GetZones();
			for (auto zone : zones) {
				vector<pair<float, float>> vertices = {
					plot->GetPosition(zone.second->GetPosX() + zone.second->GetSizeX() / 2.f, zone.second->GetPosY() + zone.second->GetSizeY() / 2.f),
					plot->GetPosition(zone.second->GetPosX() - zone.second->GetSizeX() / 2.f, zone.second->GetPosY() + zone.second->GetSizeY() / 2.f),
					plot->GetPosition(zone.second->GetPosX() - zone.second->GetSizeX() / 2.f, zone.second->GetPosY() - zone.second->GetSizeY() / 2.f),
					plot->GetPosition(zone.second->GetPosX() + zone.second->GetSizeX() / 2.f, zone.second->GetPosY() - zone.second->GetSizeY() / 2.f),
				};

				rgb color;
				if (chunkColors.find(zone.first) != chunkColors.end()) {
					color = chunkColors[zone.first];
				}
				else {
					color = { (unsigned char)GetRandom(128), (unsigned char)GetRandom(128), (unsigned char)GetRandom(128) };
					chunkColors[zone.first] = color;
				}

				float x1 = sizeX / 2 + (vertices[0].first - cameraX) * (8 / steps[zoom]);
				float y1 = sizeY / 2 + (vertices[0].second - cameraY) * (8 / steps[zoom]);
				float x2 = sizeX / 2 + (vertices[1].first - cameraX) * (8 / steps[zoom]);
				float y2 = sizeY / 2 + (vertices[1].second - cameraY) * (8 / steps[zoom]);
				float x3 = sizeX / 2 + (vertices[2].first - cameraX) * (8 / steps[zoom]);
				float y3 = sizeY / 2 + (vertices[2].second - cameraY) * (8 / steps[zoom]);
				float x4 = sizeX / 2 + (vertices[3].first - cameraX) * (8 / steps[zoom]);
				float y4 = sizeY / 2 + (vertices[3].second - cameraY) * (8 / steps[zoom]);

				setColor(color.r, color.g, color.b);
				putTriangle(x1, y1, x2, y2, x3, y3, SOLID_FILL);
				putTriangle(x1, y1, x3, y3, x4, y4, SOLID_FILL);
			}

			auto buildings = plot->GetBuildings();
			for (auto building : buildings) {
				vector<pair<float, float>> vertices = {
					plot->GetPosition(building.second->GetPosX() + building.second->GetSizeX() / 2.f, building.second->GetPosY() + building.second->GetSizeY() / 2.f),
					plot->GetPosition(building.second->GetPosX() - building.second->GetSizeX() / 2.f, building.second->GetPosY() + building.second->GetSizeY() / 2.f),
					plot->GetPosition(building.second->GetPosX() - building.second->GetSizeX() / 2.f, building.second->GetPosY() - building.second->GetSizeY() / 2.f),
					plot->GetPosition(building.second->GetPosX() + building.second->GetSizeX() / 2.f, building.second->GetPosY() - building.second->GetSizeY() / 2.f),
				};

				rgb color;
				if (chunkColors.find(building.first) != chunkColors.end()) {
					color = chunkColors[building.first];
				}
				else {
					color = { (unsigned char)GetRandom(128), (unsigned char)GetRandom(128), (unsigned char)GetRandom(128) };
					chunkColors[building.first] = color;
				}

				float x1 = sizeX / 2 + (vertices[0].first - cameraX) * (8 / steps[zoom]);
				float y1 = sizeY / 2 + (vertices[0].second - cameraY) * (8 / steps[zoom]);
				float x2 = sizeX / 2 + (vertices[1].first - cameraX) * (8 / steps[zoom]);
				float y2 = sizeY / 2 + (vertices[1].second - cameraY) * (8 / steps[zoom]);
				float x3 = sizeX / 2 + (vertices[2].first - cameraX) * (8 / steps[zoom]);
				float y3 = sizeY / 2 + (vertices[2].second - cameraY) * (8 / steps[zoom]);
				float x4 = sizeX / 2 + (vertices[3].first - cameraX) * (8 / steps[zoom]);
				float y4 = sizeY / 2 + (vertices[3].second - cameraY) * (8 / steps[zoom]);

				setColor(color.r, color.g, color.b);
				putTriangle(x1, y1, x2, y2, x3, y3, SOLID_FILL);
				putTriangle(x1, y1, x3, y3, x4, y4, SOLID_FILL);
			}
		}
	}

	setColor(255, 255, 255);
	for (auto road : roadnet->GetConnections()) {
		float x1 = road.GetV1().GetX();
		float y1 = road.GetV1().GetY();
		float x2 = road.GetV2().GetX();
		float y2 = road.GetV2().GetY();
		x1 = sizeX / 2 + (x1 - cameraX) * (8 / steps[zoom]);
		y1 = sizeY / 2 + (y1 - cameraY) * (8 / steps[zoom]);
		x2 = sizeX / 2 + (x2 - cameraX) * (8 / steps[zoom]);
		y2 = sizeY / 2 + (y2 - cameraY) * (8 / steps[zoom]);
		putLine(x1, y1, x2, y2, SOLID_LINE);
	}

}

void resize(int x, int y) {
	sizeX = x;
	sizeY = y - 20;

	updateGraph(cameraX, cameraY, zoom);
}

void sgSetup() {
	initWindow(sizeX, sizeY, "Map", BIT_MAP);
	resizeFuntion(resize);

	::map->InitTerrains();
	::map->InitRoadnets();
	::map->InitZones();
	::map->InitBuildings();
	::map->InitComponents();
	::map->InitRooms();
	::map->ReadConfigs(REPLACE_PATH("../Resources/configs/config_map.json"));

	populace->InitAssets();
	populace->InitJobs();
	populace->InitNames();
	populace->InitSchedulers();
	populace->ReadConfigs(REPLACE_PATH("../Resources/configs/config_populace.json"));

	society->InitCalendars();
	society->InitOrganizations();
	society->ReadConfigs(REPLACE_PATH("../Resources/configs/config_society.json"));

	story->InitEvents();
	story->InitChanges();

	try {
		populace->Init(::map->Init(3, 3));
		::map->Checkin(populace->GetCitizens(), populace->GetTime());
		society->Init(::map, populace);
		populace->Schedule();
	}
	catch (exception e) {
		debugf("%s", e.what());
	}

	cameraX = ::map->GetSize().first / 2;
	cameraY = ::map->GetSize().second / 2;
}

void sgLoop() {
	try {
		bitMap* bmp = (bitMap*)malloc(sizeof(bitMap));
		bmp->data = NULL;
		int diff = 8;
		int key;
		if (biosKey(1)) {
			key = biosKey(0);
			switch (key) {
			case SG_UP:
			case 'w':
				cameraY -= steps[zoom];
				if (cameraY < 0) {
					cameraY += steps[zoom];
					break;
				}
				getImage(0, 0, sizeX - 1, sizeY - 1 - diff, bmp);
				putImage(0, diff, bmp, COPY_PUT);
				updateGraph(cameraX, cameraY, zoom, 0.0f, sizeX, 0.0f, diff);
				break;
			case SG_DOWN:
			case 's':
				cameraY += steps[zoom];
				if (cameraY > ::map->GetSize().second) {
					cameraY -= steps[zoom];
					break;
				}
				getImage(0, diff, sizeX - 1, sizeY - 1, bmp);
				putImage(0, 0, bmp, COPY_PUT);
				updateGraph(cameraX, cameraY, zoom, 0, sizeX, sizeY - diff, sizeY);
				break;
			case SG_LEFT:
			case 'a':
				cameraX -= steps[zoom];
				if (cameraX < 0) {
					cameraX += steps[zoom];
					break;
				}
				getImage(0, 0, sizeX - 1 - diff, sizeY - 1, bmp);
				putImage(diff, 0, bmp, COPY_PUT);
				updateGraph(cameraX, cameraY, zoom, 0, diff, 0, sizeY);
				break;
			case SG_RIGHT:
			case 'd':
				cameraX += steps[zoom];
				if (cameraX > ::map->GetSize().first) {
					cameraX -= steps[zoom];
					break;
				}
				getImage(diff, 0, sizeX - 1, sizeY - 1, bmp);
				putImage(0, 0, bmp, COPY_PUT);
				updateGraph(cameraX, cameraY, zoom, sizeX - diff, sizeX, 0, sizeY);
				break;
			case ' ':
				dispMode = (dispMode + 1) % DISP_END;
				updateGraph(cameraX, cameraY, zoom);
				break;
			default:
				break;
			}
		}
		if (bmp->data)free(bmp->data);
		free(bmp);

		setColor(255, 255, 255);
		putQuad(0, sizeY, sizeX - 1, sizeY + 20 - 1, SOLID_FILL);
		auto pos = mousePos();
		int posX = cameraX + (pos.x - sizeX / 2) / (8 / steps[zoom]);
		int posY = cameraY + (pos.y - sizeY / 2) / (8 / steps[zoom]);
		auto element = ::map->GetElement(posX, posY);
		if (element && element->GetZone().size() > 0) {
			setColor(0, 0, 0);
			putString(element->GetZone().data(), 0, sizeY);
		}
		else if (element && element->GetBuilding().size() > 0) {
			setColor(0, 0, 0);
			putString(element->GetBuilding().data(), 0, sizeY);
		}

		vec3i mouse;
		setColor(0, 0, 0);
		if (biosMouse(1).z) {
			mouse = biosMouse(0);
			if (mouse.z == SG_LEFT_BUTTON) {
				int posX = cameraX + (mouse.x - sizeX / 2) / (8 / steps[zoom]);
				int posY = cameraY + (mouse.y - sizeY / 2) / (8 / steps[zoom]);
				auto element = ::map->GetElement(posX, posY);
				if (element && element->GetZone().size() > 0) {
					auto zone = ::map->GetZone(element->GetZone());
					if (zone) {
						if (currentZone) {
							closeWindow(zoneWindow);
						}
						zoneWindow = createParamWindow(
							round(zone->GetSizeX()) * 20 + 200, round(zone->GetSizeY()) * 20,
							element->GetZone().data(), BIT_MAP, zoneSetup, zoneLoop, zone.get());
					}
				}
				else if (element && element->GetBuilding().size() > 0) {
					auto building = ::map->GetBuilding(element->GetBuilding());
					if (building) {
						if (currentBuilding) {
							closeWindow(buildingWindow);
						}
						buildingWindow = createParamWindow(
							round(building->GetSizeX()) * 20 + 200, round(building->GetSizeY()) * 20,
							element->GetBuilding().data(), BIT_MAP, buildingSetup, buildingLoop, building.get());
					}
				}
			}
			else {
				if (mouse.z == SG_MIDDLE_BUTTON_UP)
					zoom++;
				if (mouse.z == SG_MIDDLE_BUTTON_DOWN)
					zoom--;
				zoom = clamp(zoom, 0, 3);
				updateGraph(cameraX, cameraY, zoom);
			}
		}
		clearMouseBuffer();
	}
	catch (exception e) {
		debugf("%s", e.what());
	}
}
