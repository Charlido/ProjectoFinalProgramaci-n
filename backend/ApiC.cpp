#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <mutex>
#include "httplib.h"    
#include "json.hpp"     
#include "vidrio.h"     

#pragma comment(lib, "ws2_32.lib") // Habilita la red en Visual Studio

using namespace std;
using namespace httplib;
using json = nlohmann::json;

// Base de datos en memoria para el taller
vector<Vidrio> vidrios_db;
int proximo_id = 1;
mutex db_mutex;
const string archivo_datos = "inventario_vidrios.dat";

void guardar_vidrios_con_lock_adquirido() {
    json j_array = json::array();
    for (const auto& v : vidrios_db) {
        j_array.push_back(v);
    }

    ofstream archivo(archivo_datos);
    if (archivo.is_open()) {
        archivo << j_array.dump(4);
        archivo.close();
    }
    else {
        cerr << "Error al abrir " << archivo_datos << " para escritura." << endl;
    }
}

void cargar_vidrios() {
    lock_guard<mutex> lock(db_mutex);

    ifstream archivo(archivo_datos);
    if (archivo.is_open()) {
        try {
            json j_array_leido;
            archivo >> j_array_leido;
            if (j_array_leido.is_array()) {
                for (const auto& j_vidrio : j_array_leido) {
                    Vidrio v = j_vidrio.get<Vidrio>();
                    vidrios_db.push_back(v);
                    if (v.id >= proximo_id) {
                        proximo_id = v.id + 1;
                    }
                }
            }
            cout << "Inventario cargado desde " << archivo_datos << endl;
        }
        catch (json::parse_error& e) {
            cerr << "Error al parsear JSON desde " << archivo_datos << ": " << e.what() << endl;
        }
        catch (json::type_error& e) {
            cerr << "Error de tipo JSON desde " << archivo_datos << ": " << e.what() << endl;
        }
        archivo.close();
    }
    else {
        cout << "No se encontro inventario previo. Iniciando base de datos vacia." << endl;
    }

    if (vidrios_db.empty() && proximo_id < 1) {
        proximo_id = 1;
    }
    else if (!vidrios_db.empty()) {
        int max_id = 0;
        for (const auto& v : vidrios_db) {
            if (v.id > max_id) max_id = v.id;
        }
        if (proximo_id <= max_id) {
            proximo_id = max_id + 1;
        }
    }
}

int main(void) {
    Server svr;

    cargar_vidrios();

    // ---------------------------------------------------------
    // Endpoint POST: Registrar material nuevo en el taller
    // ---------------------------------------------------------
   
    svr.Post("/vidrio", [&](const Request& req, Response& res) {
        try {
            json j_body = json::parse(req.body);

 
            if (!j_body.contains("tipo") || !j_body.contains("color") || !j_body.contains("grosor_mm") || !j_body.contains("dimensiones") || !j_body.contains("cantidad") || !j_body.contains("fecha_ingreso")) {
                res.status = 400;
                res.set_content("Faltan campos obligatorios: tipo, color, grosor_mm, dimensiones, cantidad, fecha_ingreso", "text/plain; charset=utf-8");
                return;
            }

            Vidrio v_nuevo = j_body.get<Vidrio>();

            lock_guard<mutex> lock(db_mutex);
            v_nuevo.id = proximo_id++;
            vidrios_db.push_back(v_nuevo);
            guardar_vidrios_con_lock_adquirido();

            json j_respuesta = v_nuevo;
            res.set_content(j_respuesta.dump(4), "application/json; charset=utf-8");
            res.status = 201;
        }
        catch (const exception& e) {
            res.status = 500;
            res.set_content("Error interno: " + string(e.what()), "text/plain; charset=utf-8");
        }
        });

    // ---------------------------------------------------------
    // Endpoint GET: Obtener todo el inventario de vidrios
    // ---------------------------------------------------------
    svr.Get("/vidrios", [&](const Request& req, Response& res) {
        lock_guard<mutex> lock(db_mutex);
        json j_array_respuesta = vidrios_db;
        res.set_content(j_array_respuesta.dump(4), "application/json; charset=utf-8");
        });

    // ---------------------------------------------------------
    // Endpoint GET: Buscar un vidrio especifico por ID
    // ---------------------------------------------------------
    svr.Get(R"(/vidrio/(\d+))", [&](const Request& req, Response& res) {
        int id_buscado = stoi(req.matches[1].str());

        lock_guard<mutex> lock(db_mutex);
        auto it = find_if(vidrios_db.begin(), vidrios_db.end(),
            [id_buscado](const Vidrio& v) { return v.id == id_buscado; });

        if (it != vidrios_db.end()) {
            json j_respuesta = *it;
            res.set_content(j_respuesta.dump(4), "application/json; charset=utf-8");
        }
        else {
            res.status = 404;
            res.set_content("Registro no encontrado", "text/plain; charset=utf-8");
        }
        });

    // ---------------------------------------------------------
    // Endpoint PUT: Actualizar inventario (ej. despues de un corte)
    // ---------------------------------------------------------
    svr.Put(R"(/vidrio/(\d+))", [&](const Request& req, Response& res) {
        int id_buscado = stoi(req.matches[1].str());

        try {
            json j_actualizacion = json::parse(req.body);

            lock_guard<mutex> lock(db_mutex);
            auto it = find_if(vidrios_db.begin(), vidrios_db.end(),
                [id_buscado](const Vidrio& v) { return v.id == id_buscado; });

            if (it != vidrios_db.end()) {
                if (j_actualizacion.contains("tipo")) it->tipo = j_actualizacion["tipo"].get<string>();
                if (j_actualizacion.contains("color")) it->color = j_actualizacion["color"].get<string>();
                if (j_actualizacion.contains("grosor_mm")) it->grosor_mm = j_actualizacion["grosor_mm"].get<int>();
                if (j_actualizacion.contains("dimensiones")) it->dimensiones = j_actualizacion["dimensiones"].get<string>();
                if (j_actualizacion.contains("cantidad")) it->cantidad = j_actualizacion["cantidad"].get<int>();

                guardar_vidrios_con_lock_adquirido();

                json j_respuesta = *it;
                res.set_content(j_respuesta.dump(4), "application/json; charset=utf-8");
            }
            else {
                res.status = 404;
                res.set_content("Registro no encontrado", "text/plain; charset=utf-8");
            }
        }
        catch (const exception& e) {
            res.status = 500;
            res.set_content("Error interno: " + string(e.what()), "text/plain; charset=utf-8");
        }
        });

    // ---------------------------------------------------------
    // Endpoint DELETE: Eliminar un material del registro
    // ---------------------------------------------------------
    svr.Delete(R"(/vidrio/(\d+))", [&](const Request& req, Response& res) {
        int id_buscado = stoi(req.matches[1].str());

        lock_guard<mutex> lock(db_mutex);
        auto it = find_if(vidrios_db.begin(), vidrios_db.end(),
            [id_buscado](const Vidrio& v) { return v.id == id_buscado; });

        if (it != vidrios_db.end()) {
            vidrios_db.erase(it);
            guardar_vidrios_con_lock_adquirido();
            res.status = 204;
        }
        else {
            res.status = 404;
            res.set_content("Registro no encontrado", "text/plain; charset=utf-8");
        }
        });

    // ---------------------------------------------------------
    // SOLUCION CORS: Limpio y sin duplicados
    // ---------------------------------------------------------

    // 1. Atrapamos la peticion de permiso del navegador y le damos luz verde (200)
    svr.Options(R"(.*)", [](const Request& req, Response& res) {
        res.status = 200;
        });

    // 2. Justo antes de que cualquier respuesta salga hacia el HTML, pegamos los permisos UNA sola vez
    svr.set_post_routing_handler([](const auto& req, auto& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        });

    // Iniciar servidor
    cout << "Servidor de Inventario de Vidrios iniciando en el puerto 8080" << endl;
    svr.listen("0.0.0.0", 8080);

    return 0;

}