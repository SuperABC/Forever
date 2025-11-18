#include "zone_base.h"

#include "../common/error.h"

#include <algorithm>


using namespace std;

void Zone::SetParent(shared_ptr<Plot> plot) {
    parentPlot = plot;
}

shared_ptr<Plot> Zone::GetParent() const {
    return parentPlot;
}

shared_ptr<Building> Zone::GetBuilding(string name) {
    return buildings[name];
}

unordered_map<string, shared_ptr<Building>>& Zone::GetBuildings() {
    return buildings;
}

void Zone::AddBuildings(BuildingFactory* factory, vector<pair<string, float>> list) {
    float acreageTmp = 0.f;
    int attempt = 0;
    for (size_t i = 0; i < list.size(); i++) {
        if (acreageTmp >= GetAcreage() || attempt > 16)break;

        shared_ptr<Building> building = factory->CreateBuilding(list[i].first);
        if (!building) {
            attempt++;
            i--;
            continue;
        }

        float acreageBuilding = building->RandomAcreage() * list[i].second;
        float acreageMin = building->GetAcreageMin() * list[i].second;
        float acreageMax = building->GetAcreageMax() * list[i].second;
        if (GetAcreage() - acreageTmp < acreageMin) {
            attempt++;
            i--;
            continue;
        }
        else if (GetAcreage() - acreageTmp < acreageBuilding) {
            acreageBuilding = GetAcreage() - acreageTmp;
        }

        acreageTmp += acreageBuilding;
        building->SetAcreage(acreageBuilding);
        if(buildings.find(building->GetName()) != buildings.end()) {
            THROW_EXCEPTION(InvalidConfigException, "Duplicate building name: " + building->GetName() + ".\n");
		}
        buildings[building->GetName()] = building;
    }
}

void Zone::ArrangeBuildings() {
    if (buildings.empty()) return;

    float acreageTotal = GetAcreage();
    float acreageUsed = 0.f;

    for (const auto& building : buildings) {
        acreageUsed += building.second->GetAcreage();
    }
    float acreageRemain = acreageTotal - acreageUsed;

    bool acreageAllocate = false;
    if (acreageRemain > 0) {
        for (auto& building : buildings) {
            float acreageTmp = building.second->GetAcreage();
            float acreageMax = building.second->GetAcreageMax();
            float acreageMin = building.second->GetAcreageMin();

            float acreageExpand = acreageMax - acreageTmp;

            if (acreageExpand > acreageRemain && acreageRemain > 0) {
                float acreageNew = acreageTmp + acreageRemain;
                if (acreageNew >= acreageMin && acreageNew <= acreageMax) {
                    building.second->SetAcreage(acreageNew);
                    acreageUsed += acreageRemain;
                    acreageRemain = 0.f;
                    acreageAllocate = true;
                    break;
                }
            }
        }
    }

    vector<shared_ptr<Rect>> elements;
    if (acreageRemain > 0 && !acreageAllocate) {
        auto emptyRect = make_shared<Plot>();
        emptyRect->SetAcreage(acreageRemain);
        elements.push_back(emptyRect);
    }

    for (const auto& building : buildings) {
        elements.push_back(static_pointer_cast<Rect>(building.second));
    }

    if (elements.empty()) return;

    sort(elements.begin(), elements.end(), [](shared_ptr<Rect> a, shared_ptr<Rect> b) {
        return a->GetAcreage() > b->GetAcreage();
        });

    Rect container = Rect(GetSizeX() / 2, GetSizeY() / 2, GetSizeX(), GetSizeY());
    if (elements.size() == 1) {
        elements[0]->SetPosition(container.GetPosX(), container.GetPosY(), container.GetSizeX(), container.GetSizeY());
    }
    else {
        class Chunk : public Rect {
        public:
            Chunk(shared_ptr<Rect> r1, shared_ptr<Rect> r2) : r1(r1), r2(r2) { acreage = r1->GetAcreage() + r2->GetAcreage(); }
            shared_ptr<Rect> r1, r2;
        };
        while (elements.size() > 2) {
            shared_ptr<Chunk> tmp = make_shared<Chunk>(elements[elements.size() - 1], elements[elements.size() - 2]);
            elements.pop_back();
            int i = (int)elements.size() - 2;
            for (; i >= 0; i--) {
                if (tmp->GetAcreage() > elements[i]->GetAcreage()) {
                    elements[i + 1] = elements[i];
                }
                else {
                    elements[i + 1] = tmp;
                    break;
                }
            }
            if (i < 0)elements[0] = tmp;
        }

        if (container.GetSizeX() > container.GetSizeY()) {
            if (GetRandom(2)) {
                int divX = int(container.GetLeft() +
                    (container.GetRight() - container.GetLeft()) * elements[0]->GetAcreage() / container.GetAcreage());
                if (abs(divX - container.GetLeft()) < 2)divX = (int)container.GetLeft();
                if (abs(divX - container.GetRight()) < 2)divX = (int)container.GetRight();
                elements[0]->SetVertices(container.GetLeft(), container.GetBottom(), (float)divX, container.GetTop());
                elements[1]->SetVertices((float)divX, container.GetBottom(), container.GetRight(), container.GetTop());
            }
            else {
                int divX = int(container.GetLeft() +
                    (container.GetRight() - container.GetLeft()) * elements[1]->GetAcreage() / container.GetAcreage());
                if (abs(divX - container.GetLeft()) < 2)divX = (int)container.GetLeft();
                if (abs(divX - container.GetRight()) < 2)divX = (int)container.GetRight();
                elements[1]->SetVertices(container.GetLeft(), container.GetBottom(), (float)divX, container.GetTop());
                elements[0]->SetVertices((float)divX, container.GetBottom(), container.GetRight(), container.GetTop());
            }
        }
        else {
            if (GetRandom(2)) {
                int divY = int(container.GetBottom() +
                    (container.GetTop() - container.GetBottom()) * elements[0]->GetAcreage() / container.GetAcreage());
                if (abs(divY - container.GetBottom()) < 2)divY = (int)container.GetBottom();
                if (abs(divY - container.GetTop()) < 2)divY = (int)container.GetTop();
                elements[0]->SetVertices(container.GetLeft(), container.GetBottom(), container.GetRight(), (float)divY);
                elements[1]->SetVertices(container.GetLeft(), (float)divY, container.GetRight(), container.GetTop());
            }
            else {
                int divY = int(container.GetBottom() +
                    (container.GetTop() - container.GetBottom()) * elements[1]->GetAcreage() / container.GetAcreage());
                if (abs(divY - container.GetBottom()) < 2)divY = (int)container.GetBottom();
                if (abs(divY - container.GetTop()) < 2)divY = (int)container.GetTop();
                elements[1]->SetVertices(container.GetLeft(), container.GetBottom(), container.GetRight(), (float)divY);
                elements[0]->SetVertices(container.GetLeft(), (float)divY, container.GetRight(), container.GetTop());
            }
        }

        while (elements.size() > 0) {
            auto tmp = elements.back();
            elements.pop_back();
            if (auto chunk = dynamic_pointer_cast<Chunk>(tmp)) {
                shared_ptr<Rect> rect1 = chunk->r1;
                shared_ptr<Rect> rect2 = chunk->r2;

                if (tmp->GetAcreage() > 0) {
                    if (tmp->GetSizeX() > tmp->GetSizeY()) {
                        if (GetRandom(2)) {
                            int divX = int(tmp->GetLeft() +
                                tmp->GetSizeX() * rect1->GetAcreage() / tmp->GetAcreage());
                            if (abs(divX - tmp->GetLeft()) < 2)divX = (int)tmp->GetLeft();
                            if (abs(divX - tmp->GetRight()) < 2)divX = (int)tmp->GetRight();
                            rect1->SetVertices(tmp->GetLeft(), tmp->GetBottom(), (float)divX, tmp->GetTop());
                            rect2->SetVertices((float)divX, tmp->GetBottom(), tmp->GetRight(), tmp->GetTop());
                        }
                        else {
                            int divX = int(tmp->GetLeft() +
                                tmp->GetSizeX() * rect2->GetAcreage() / tmp->GetAcreage());
                            if (abs(divX - tmp->GetLeft()) < 2)divX = (int)tmp->GetLeft();
                            if (abs(divX - tmp->GetRight()) < 2)divX = (int)tmp->GetRight();
                            rect2->SetVertices(tmp->GetLeft(), tmp->GetBottom(), (float)divX, tmp->GetTop());
                            rect1->SetVertices((float)divX, tmp->GetBottom(), tmp->GetRight(), tmp->GetTop());
                        }
                    }
                    else {
                        if (GetRandom(2)) {
                            int divY = int(tmp->GetBottom() +
                                tmp->GetSizeY() * rect1->GetAcreage() / tmp->GetAcreage());
                            if (abs(divY - tmp->GetBottom()) < 2)divY = (int)tmp->GetBottom();
                            if (abs(divY - tmp->GetTop()) < 2)divY = (int)tmp->GetTop();
                            rect1->SetVertices(tmp->GetLeft(), tmp->GetBottom(), tmp->GetRight(), (float)divY);
                            rect2->SetVertices(tmp->GetLeft(), (float)divY, tmp->GetRight(), tmp->GetTop());
                        }
                        else {
                            int divY = int(tmp->GetBottom() +
                                tmp->GetSizeY() * rect2->GetAcreage() / tmp->GetAcreage());
                            if (abs(divY - tmp->GetBottom()) < 2)divY = (int)tmp->GetBottom();
                            if (abs(divY - tmp->GetTop()) < 2)divY = (int)tmp->GetTop();
                            rect2->SetVertices(tmp->GetLeft(), tmp->GetBottom(), tmp->GetRight(), (float)divY);
                            rect1->SetVertices(tmp->GetLeft(), (float)divY, tmp->GetRight(), tmp->GetTop());
                        }
                    }
                    if (dynamic_pointer_cast<Chunk>(rect1))elements.push_back(rect1);
                    if (dynamic_pointer_cast<Chunk>(rect2))elements.push_back(rect2);
                }
            }
        }
    }
}


void ZoneFactory::RegisterZone(const string& id,
    function<unique_ptr<Zone>()> creator,  GeneratorFunc generator) {
    registries[id] = creator;
    configs[id] = false;
    generators[id] = generator;
}

unique_ptr<Zone> ZoneFactory::CreateZone(const string& id) {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool ZoneFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void ZoneFactory::SetConfig(string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}

void ZoneFactory::GenerateAll(const vector<shared_ptr<Plot>>& plots, BuildingFactory* factory) {
    for (const auto& [id, generator] : generators) {
        if (generator && configs[id]) {
            generator(this, factory, plots);
        }
    }
}

