#pragma once
#include <string>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

struct Vidrio {
    int id = 0;
    string tipo;
    string color;
    int grosor_mm = 0;
    string dimensiones;
    int cantidad = 0;
    string fecha_ingreso;
};

inline void from_json(const json& j, Vidrio& vidrio_out) {
    if (j.contains("id")) j.at("id").get_to(vidrio_out.id);
    if (j.contains("tipo")) j.at("tipo").get_to(vidrio_out.tipo);
    if (j.contains("color")) j.at("color").get_to(vidrio_out.color);
    if (j.contains("grosor_mm")) j.at("grosor_mm").get_to(vidrio_out.grosor_mm);
    if (j.contains("dimensiones")) j.at("dimensiones").get_to(vidrio_out.dimensiones);
    if (j.contains("cantidad")) j.at("cantidad").get_to(vidrio_out.cantidad);
    if (j.contains("fecha_ingreso")) j.at("fecha_ingreso").get_to(vidrio_out.fecha_ingreso); 
}

inline void to_json(json& j, const Vidrio& vidrio_in) {
    j = json{
        {"id", vidrio_in.id},
        {"tipo", vidrio_in.tipo},
        {"color", vidrio_in.color},
        {"grosor_mm", vidrio_in.grosor_mm},
        {"dimensiones", vidrio_in.dimensiones},
        {"cantidad", vidrio_in.cantidad},
        {"fecha_ingreso", vidrio_in.fecha_ingreso} 
    };
}   