#include "app.h"
#include <stdexcept>
#include <iostream>
#include <string>

namespace app{
namespace {
    // Константы для строковых кодов направлений
    constexpr const char* DIR_UP    = "U";
    constexpr const char* DIR_DOWN  = "D";
    constexpr const char* DIR_LEFT  = "L";
    constexpr const char* DIR_RIGHT = "R";

    // Конвертация Direction в строковый код
    static const char* DirectionToChar(Direction dir) {
        switch (dir) {
            case Direction::NORTH: return DIR_UP;
            case Direction::SOUTH: return DIR_DOWN;
            case Direction::WEST:  return DIR_LEFT;
            case Direction::EAST:  return DIR_RIGHT;
            default:               return "";
        }
    }

    // Конвертация строкового кода в Direction
    static Direction CharToDirection(const std::string& s) {
        if (s == DIR_UP)    return Direction::NORTH;
        if (s == DIR_DOWN)  return Direction::SOUTH;
        if (s == DIR_LEFT)  return Direction::WEST;
        if (s == DIR_RIGHT) return Direction::EAST;
        throw std::invalid_argument("Unknown direction code: " + s);
    }
}

/* ------------------------ GetMapUseCase ----------------------------------- */

std::string GetMapUseCase::MakeMapDescription(const Map* map){
    json::object map_description;

    map_description["id"] = *(map->GetId());
    map_description["name"] =  map->GetName();
    map_description["roads"] = GetRoadsInJSON(map->GetRoads());
    map_description["buildings"] = GetBuildingsInJSON(map->GetBuildings());
    map_description["offices"] = GetOfficesInJSON(map->GetOffices());
    map_description["lootTypes"] = GetLootTypesInJSON(map->GetLootTypes());
    unsigned bag_cap = map->GetBagCapacity();
    if(bag_cap == 3){
        map_description["bagCapacity"] = map->GetBagCapacity();
    }


    return json::serialize(map_description); 
}


json::array GetMapUseCase::GetRoadsInJSON(const Map::Roads& roads){
    json::array result;

    for(const Road& road : roads){
        json::object obj;
        obj["x0"] = road.GetStart().x;
        obj["y0"] = road.GetStart().y;
        if(road.IsHorizontal()){
            obj["x1"] = road.GetEnd().x;
        } else if(road.IsVertical()){
            obj["y1"] = road.GetEnd().y;
        } else {
            break;
        }

        result.push_back(obj); 
    }
    

    return result;
}

json::array GetMapUseCase::GetBuildingsInJSON(const Map::Buildings& buildings){
    json::array result;

    for(const Building& building : buildings){
        json::object obj;
        obj["x"] = building.GetBounds().position.x;
        obj["y"] = building.GetBounds().position.y;
        obj["w"] = building.GetBounds().size.width;
        obj["h"] = building.GetBounds().size.height;
        result.push_back(obj);
    }

    return result;
}

json::array GetMapUseCase::GetOfficesInJSON(const Map::Offices& offices){
    json::array result;

    for(const Office& office : offices){
        json::object obj;
        obj["id"] = *(office.GetId());
        obj["x"] = office.GetPosition().x;
        obj["y"] = office.GetPosition().y;
        obj["offsetX"] = office.GetOffset().dx;
        obj["offsetY"] = office.GetOffset().dy;
        result.push_back(obj);
    }

    return result;
}

json::array GetMapUseCase::GetLootTypesInJSON(const Map::LootTypes& loot_types){
    json::array result;

    for(const LootType& lt : loot_types){
        json::object obj;
        if(lt.name.has_value()){
            obj["name"] = *lt.name;
        }
        if(lt.file.has_value()){
            obj["file"] = *lt.file;
        }
        if(lt.type.has_value()){
            obj["type"] = *lt.type;
        }
        if(lt.rotation.has_value()){
            obj["rotation"] = *lt.rotation;
        }
        if(lt.color.has_value()){
            obj["color"] = *lt.color;
        }
        if(lt.scale.has_value()){
            obj["scale"] = *lt.scale;
        }
        if(lt.value.has_value()){
            obj["value"] = *lt.value;
        }

        result.push_back(obj);
    }

    return result;
}

/* ------------------------ ListMapsUseCase ----------------------------------- */

std::string ListMapsUseCase::MakeMapsList(const Game::Maps& maps){
    json::array map_list;
    for(const Map& map : maps){
        json::object obj;
        obj["id"] = *(map.GetId());
        obj["name"] = map.GetName();
        map_list.push_back(obj);
    }

    return json::serialize(map_list);
}

/* ------------------------ GameUseCase ----------------------------------- */

std::string GameUseCase::JoinGame(const std::string& user_name, const std::string& str_map_id, 
                        Game& game, bool is_random_spawn_enabled){
    using namespace std::literals;
    Map::Id map_id(str_map_id);

    GameSession* session = game.SessionIsExists(map_id);
    if(session == nullptr){
        session = game.AddSession(map_id);
    }

    Dog::Name dog_name(user_name);
    Dog::Position dog_pos = (is_random_spawn_enabled) 
        ? Dog::Position(Map::GetRandomPos(game.FindMap(map_id)->GetRoads())) 
        : Dog::Position(Map::GetFirstPos(game.FindMap(map_id)->GetRoads()));
    Dog::Speed dog_speed({0, 0});
    Direction dog_dir = Direction::NORTH;

    Dog* dog = session->AddDog(auto_counter_, dog_name, dog_pos, 
                                        dog_speed, dog_dir);
    /*
        С появлением нового игрока в сессии,
        нужно обновить количество потерянных объектов
    */
    session->UpdateLoot(session->GetDogs().size() - session->GetLootObjects().size());
    Player& player = players_.Add(auto_counter_, Player::Name(user_name), 
                                        dog, session);
    ++auto_counter_;

    Token token = tokens_.AddPlayer(player);
    
    json::object json_body;
    json_body["authToken"] = *token;
    json_body["playerId"] = player.GetId();

    return json::serialize(json_body);   
}

json::array GameUseCase::GetBagItems(const Dog::Bag& bag_items){
    json::array items;
    for(const Loot& loot : *bag_items){
        json::object loot_desc;
        loot_desc["id"] = loot.id;
        loot_desc["type"] = loot.type;

        items.push_back(loot_desc);
    }   

    return items;
};

json::object GameUseCase::GetPlayers(const PlayerTokens::PlayersInSession& players_in_session){
    json::object players;

    for(const Player* player : players_in_session){
        json::object player_attributes;

        const PairDouble& pos = *(player->GetDog()->GetPosition());
        player_attributes["pos"] = {pos.x, pos.y};
        
        const PairDouble& speed = *(player->GetDog()->GetSpeed());
        player_attributes["speed"] = {speed.x, speed.y};

        Direction dir = player->GetDog()->GetDirection();
        player_attributes["dir"] = DirectionToChar(dir);

        player_attributes["bag"] = GetBagItems(player->GetDog()->GetBag());
        player_attributes["score"] = player->GetDog()->GetScore();

        players[std::to_string(player->GetId())] = player_attributes;
    }

    return players;
}

json::object GameUseCase::GetLostObjects(const std::deque<Loot>& loots){
    json::object lost_objects;
    
    for(const Loot& loot : loots){
        json::object loot_decs;

        loot_decs["type"] = loot.type;
        json::array pos = { loot.pos.x, loot.pos.y };
        loot_decs["pos"] = pos;

        lost_objects[std::to_string(loot.id)] = loot_decs;
    }

    return lost_objects;
}

std::string GameUseCase::GetGameState(const Token& token) const{
    json::object result;
    const GameSession* session = tokens_.FindPlayerByToken(token)->GetSession();

    result["players"] = GetPlayers(tokens_.GetPlayersBySession(session));
    result["lostObjects"] = GetLostObjects(session->GetLootObjects());

    return json::serialize(result);
}

std::string GameUseCase::SetAction(const json::object& action, const Token& token){
    Player* player = tokens_.FindPlayerByToken(token);
    double dog_speed = player->GetSession()->GetMap()->GetDogSpeed();

    Direction new_dir;
    Dog::Speed new_speed({0, 0});
    
    std::string move_code = action.at("move").as_string().c_str();
    Direction dir_enum = CharToDirection(move_code);

    switch(dir_enum) {
        case Direction::NORTH:
            new_speed = Dog::Speed({0, -dog_speed});
            new_dir   = Direction::NORTH;
            break;
        case Direction::SOUTH:
            new_speed = Dog::Speed({0, dog_speed});
            new_dir   = Direction::SOUTH;
            break;
        case Direction::WEST:
            new_speed = Dog::Speed({-dog_speed, 0});
            new_dir   = Direction::WEST;
            break;
        case Direction::EAST:
            new_speed = Dog::Speed({dog_speed, 0});
            new_dir   = Direction::EAST;
            break;
        default:
            throw std::invalid_argument("Unknown direction code: " + move_code);
    }

    player->GetDog()->SetSpeed(new_speed);
    player->GetDog()->SetDirection(new_dir);
    
    return "{}";
}

std::string GameUseCase::IncreaseTime(double delta, Game& game){
    game.UpdateGameState(delta);
    return "{}";
}

void GameUseCase::GenerateLoot(detail::Milliseconds delta, Game& game){
    game.GenerateLootInSessions(delta);
}

/* ------------------------ ListPlayersUseCase ----------------------------------- */

std::string ListPlayersUseCase::GetPlayersInJSON(const PlayerTokens::PlayersInSession& players){
    json::object player_list;
    for(const Player* player : players){
        json::object player_description;
        player_description["name"] = *(player->GetName());
        player_list[std::to_string(player->GetId())] = player_description;
    }
    return json::serialize(player_list);
}
}; //namespace app
