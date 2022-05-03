#include "v6ap.h"
#include "Archipelago.h"

#include "Graphics.h"
#include "Vlogging.h"
#include "Entity.h"

#include <json/value.h>
#include <json/writer.h>
#include <json/reader.h>
#include <vector>
#include <string>
#include <map>

bool trinketsCollected[V6AP_NUM_CHECKS];
bool trinketsPending[V6AP_NUM_CHECKS];
int door_unlock_cost = 0;
bool location_checks[V6AP_NUM_CHECKS];

std::map<int,int> map_entrances;
std::map<int,int> map_exits;

std::map<int,int> map_costs;

std::map<int,int> map_music;

void V6AP_RecvItem(int,bool);
void V6AP_CheckLocation(int);

//Multiplayer Stuff
std::map<std::string,V6Player> mp_dudes;
Json::FastWriter writer; //Needed for SetRaw. Protocols are hard...
Json::Reader reader;
AP_GetServerDataRequest allpresencerequest;
std::string raw_allpresence;
bool mpreg = false;

void V6AP_SetDoorCost(int cost) {
    door_unlock_cost = cost;
}

void V6AP_SetAreaMap(std::map<int,int> map) {
    map_entrances = map;
    for (int i = 0; i < map.size(); i++) {
        map_exits[map_entrances.at(i)] = i;
    }
}

void V6AP_SetMusicMap(std::map<int,int> map) {
    map_music = map;
}

void V6AP_SetCostMap(std::map<int,int> map) {
    map_costs = map;
}

void V6AP_ResetItems() {
    for (int i = 0; i < V6AP_NUM_CHECKS; i++) {
        location_checks[i] = false;
        trinketsCollected[i] = false;
        trinketsPending[i] = false;
    }
}

void V6AP_GenericInit() {
    AP_SetDeathLinkSupported(true);
    AP_EnableQueueItemRecvMsgs(false);
    AP_SetItemClearCallback(&V6AP_ResetItems);
    AP_SetItemRecvCallback(&V6AP_RecvItem);
    AP_SetLocationCheckedCallback(&V6AP_CheckLocation);
    AP_RegisterSlotDataIntCallback("DoorCost", &V6AP_SetDoorCost);
    AP_RegisterSlotDataMapIntIntCallback("AreaRando", &V6AP_SetAreaMap);
    AP_RegisterSlotDataMapIntIntCallback("AreaCostRando", &V6AP_SetCostMap);
    AP_RegisterSlotDataMapIntIntCallback("MusicRando", &V6AP_SetMusicMap);
    AP_Start();
}

void V6AP_Init(const char* ip, const char* player_name, const char* passwd) {
    AP_Init(ip, "VVVVVV", player_name, passwd);
    V6AP_GenericInit();
}

void V6AP_Init(const char* filename) {
    AP_Init(filename);
    V6AP_GenericInit();
}

void V6AP_SendItem(int idx) {
    if (idx >= V6AP_NUM_CHECKS || location_checks[idx]) {
        vlog_warn("V6AP: Something funky is happening...");
        return;
    }
    location_checks[idx] = true;
    AP_SendItem(idx + V6AP_ID_OFFSET);
}

bool V6AP_ItemPending() {
    for (int i = 0; i < V6AP_NUM_CHECKS; i++) {
        if (trinketsPending[i]) return true;
    }
    return false;
}

void V6AP_RecvItem(int item_id, bool notify) {
    bool* increment = notify ? trinketsPending : trinketsCollected;
    switch(item_id) {
        default:
            increment[item_id - V6AP_ID_OFFSET] = true;
            break;
    }
}

void V6AP_CheckLocation(int loc_id) {
    location_checks[loc_id - V6AP_ID_OFFSET] = true;
}

void V6AP_RecvClear() {
    int i = 0;
    for (; i < V6AP_NUM_CHECKS; i++) {
        if (trinketsPending[i]) {
            trinketsPending[i] = false;
            break;
        }
    }
    trinketsCollected[i] = true;
}

void V6AP_StoryComplete() {
    AP_StoryComplete();
}

int V6AP_GetTrinkets() {
    int c = 0;
    for (int i = 0; i < V6AP_NUM_CHECKS; i++) {
        if (trinketsCollected[i]) c++;
    }
    return c;
}

void V6AP_DeathLinkSend() {
    return AP_DeathLinkSend();
}

bool V6AP_DeathLinkPending() {
    return AP_DeathLinkPending();
}

void V6AP_DeathLinkClear() {
    AP_DeathLinkClear();
}

const bool* V6AP_Locations() {
    return location_checks;
}

const bool* V6AP_Trinkets() {
    return trinketsCollected;
}

void printMsg(std::vector<std::string> msg) {
    graphics.createtextbox(msg.at(0), -7, -7, 174, 174, 174);
    for (unsigned int i = 1; i < msg.size(); i++) {
        graphics.addline(msg.at(i));
    }
    graphics.textboxtimer(60);
}

void V6AP_PrintNext() {
    if (!AP_IsMessagePending()) return;
    std::vector<std::string> msg = AP_GetLatestMessage();
    printMsg(msg);
    AP_ClearLatestMessage();
}

void V6AP_AdjustMusic(int* x) {
    *x = map_music.at(*x);
}

int V6AP_PlayerIsEnteringArea(int x, int y) {
    switch (x) {
        case 102:
            if (y == 116 && game.roomx == 101 && game.roomy == 116) { //Get Ready to Bounce (Entrance Laboratory)
                return Areas::Laboratory;
            }
            break;
        case 108:
            if (y == 109 && game.roomx == 107 && game.roomy == 109) { //Teleporter Divot (Entrance Tower)
                return Areas::TheTower;
            }
            break;
        case 111:
            if (y == 113 && game.roomx == 111 && game.roomy == 114) { //Something filter? (Entrance Space Station 2)
                return Areas::SpaceStation2;
            }
            break;
        case 114:
            if (y == 101 && game.roomx == 113 && game.roomy == 101) { // This is how it is (Entrance Warp Zone)
                return Areas::WarpZone;
            }
            break;
    }
    return Areas::None;
}

int V6AP_PlayerIsLeavingArea(int x, int y) {
    switch (game.roomx) {
        case 102:
            if (game.roomy == 116 && x == 101 && y == 116) { //Get Ready to Bounce (Entrance Laboratory)
                return Areas::Laboratory;
            }
            break;
        case 108:
            if (game.roomy == 109 && x == 107 && y == 109) { //Teleporter Divot (Entrance Tower)
                return Areas::TheTower;
            }
            break;
        case 111:
            if (game.roomy == 113 && x == 111 && y == 114) { //Something filter? (Entrance Space Station 2)
                return Areas::SpaceStation2;
            }
            break;
        case 114:
            if (game.roomy == 101 && x == 113 && y == 101) { // This is how it is (Entrance Warp Zone)
                return Areas::WarpZone;
            }
            break;
    }
    return Areas::None;
}

void V6AP_AdjustRoom(int *x, int *y, int area, bool isExit) {
    switch (area) {
        case Areas::Laboratory:
            *x = 102 - isExit;
            *y = 116;   
            if (isExit) {
                SET_PLAYER_POS_SAFE_ENTRANCE1;
            } else {
                SET_PLAYER_POS_SAFE_EXIT1;
            }
            break;
        case Areas::TheTower:
            *x = 108 - isExit;
            *y = 109;
            if (isExit) {
                SET_PLAYER_POS_SAFE_ENTRANCE2;
            } else {
                SET_PLAYER_POS_SAFE_EXIT2;
            }
            break;
        case Areas::SpaceStation2:
            *x = 111;
            *y = 113 + isExit;
            game.gravitycontrol = 1;
            if (isExit) {
                SET_PLAYER_POS_SAFE_ENTRANCE3;
            } else {
                SET_PLAYER_POS_SAFE_EXIT3;
            }
            break;
        case Areas::WarpZone:
            *x = 114 - isExit;
            *y = 101;
            if (isExit) {
                SET_PLAYER_POS_SAFE_ENTRANCE4;
            } else {
                SET_PLAYER_POS_SAFE_EXIT4;
            }
            break;
    }
}

void V6AP_RoomAvailable(int* x, int* y) {
    int entrance = V6AP_PlayerIsEnteringArea(*x, *y);
    if (entrance == Areas::None) {
        int exit = V6AP_PlayerIsLeavingArea(*x, *y);
        V6AP_AdjustRoom(x, y, map_exits.at(exit), true);
        return;
    }
    if (door_unlock_cost != 0) {
        int cost_entrance = map_costs.at(entrance);
        for (int i = door_unlock_cost*(cost_entrance-1); i < door_unlock_cost*cost_entrance; i++) {
            if (!trinketsCollected[i]) {
                std::vector<std::string> msg;
                msg.push_back("You need Trinkets");
                msg.push_back(std::to_string((door_unlock_cost*(cost_entrance-1))+1) + " through " + std::to_string(door_unlock_cost*cost_entrance));
                msg.push_back("to continue here");
                printMsg(msg);
                V6AP_AdjustRoom(x, y, entrance, true);
                return;
            }
        }    
    }
    V6AP_AdjustRoom(x, y, map_entrances.at(entrance), false);
    return;
}

void V6AP_MPUpdatePos(int roomx, int roomy, int playerx, int playery) {
    if (!mpreg) {
        mpreg = true;
        Json::Value val;
        val = Json::arrayValue;
        val[0] = AP_GetUUID();
        AP_SetServerDataRaw("AllPresenceV6", "add", writer.write(val), writer.write(Json::arrayValue));
        return;
    }

    Json::Value newpos;
    newpos["roomX"] = roomx;
    newpos["roomY"] = roomy;
    newpos["playerX"] = playerx;
    newpos["playerY"] = playery;
    newpos["flipped"] = game.gravitycontrol;
    AP_SetServerDataRaw("PresenceV6" + std::to_string(AP_GetUUID()), "replace", writer.write(newpos), "");

    if (allpresencerequest.status == AP_RequestStatus::Done) {
        Json::Value allpresence;
        reader.parse(raw_allpresence, allpresence);
        raw_allpresence.clear();
        for(Json::Value itr : allpresence) {
            if (itr.asString() == std::to_string(AP_GetUUID())) {
                continue;
            }
            if (mp_dudes[itr.asString()].request.status == AP_RequestStatus::Done) {
                Json::Value otherdude;
                reader.parse(mp_dudes[itr.asString()].data, otherdude);
                if (otherdude != Json::nullValue) {
                    if (!(otherdude["roomX"].asInt() == roomx && otherdude["roomY"].asInt() == roomy)) {
                        if (mp_dudes[itr.asString()].entity != 0) {
                            obj.entities[mp_dudes[itr.asString()].entity].invis = true;
                        }
                    } else {
                        if (mp_dudes[itr.asString()].entity == 0) {
                            mp_dudes[itr.asString()].entity = obj.entities.size();
                            obj.entities.push_back(entclass());
                            obj.entities[mp_dudes[itr.asString()].entity].type = 12;
                            obj.entities[mp_dudes[itr.asString()].entity].colour = 25;
                            obj.entities[mp_dudes[itr.asString()].entity].gravity = false;
                            obj.entities[mp_dudes[itr.asString()].entity].rule = 6;
                            obj.entities[mp_dudes[itr.asString()].entity].ismpcrew = true;
                            obj.entities[mp_dudes[itr.asString()].entity].lerpoldxp = otherdude["playerX"].asInt();
                            obj.entities[mp_dudes[itr.asString()].entity].lerpoldyp = otherdude["playerY"].asInt();
                        }
                        if (obj.entities[mp_dudes[itr.asString()].entity].invis) {
                            // Invisible before probably means they/we just switched room. Teleport 'em
                            obj.entities[mp_dudes[itr.asString()].entity].lerpoldxp = otherdude["playerX"].asInt();
                            obj.entities[mp_dudes[itr.asString()].entity].lerpoldyp = otherdude["playerY"].asInt();
                        }
                        obj.entities[mp_dudes[itr.asString()].entity].invis = false;
                        obj.entities[mp_dudes[itr.asString()].entity].tile = 144+(otherdude["flipped"].asBool() ? 6 : 0);
                        obj.entities[mp_dudes[itr.asString()].entity].xp = otherdude["playerX"].asInt();
                        obj.entities[mp_dudes[itr.asString()].entity].yp = otherdude["playerY"].asInt();
                    }
                }
            }
            mp_dudes[itr.asString()].request.key = "PresenceV6" + itr.asString();
            mp_dudes[itr.asString()].request.data = &mp_dudes[itr.asString()].data;
            mp_dudes[itr.asString()].request.type = AP_DataType::Raw;
            AP_GetServerData(&mp_dudes[itr.asString()].request);
        }
    }

    allpresencerequest.type = AP_DataType::Raw;
    allpresencerequest.data = &raw_allpresence;
    allpresencerequest.key = "AllPresenceV6";
    AP_GetServerData(&allpresencerequest);
}