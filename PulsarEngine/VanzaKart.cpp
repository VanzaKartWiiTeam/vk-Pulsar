/**
 * VanzaKart - Main Header
 * 
 * Include principale per il modpack VanzaKart
 * NOTA: Non include ObjectCollidable per evitare conflitti con keyword "default"
 */

#include <kamek.hpp>
#include <PulsarSystem.hpp>

// Core VanzaKart includes (aggiungi qui i tuoi file)
// #include <VanzaKart/Features/...>
// #include <VanzaKart/UI/...>

// NOTA IMPORTANTE:
// Se hai bisogno di ObjectCollidable, NON includerlo direttamente qui.
// Il file ObjectCollidable.hpp ha un parametro chiamato "default" che
// è una keyword C++ e causa errori di compilazione.
// 
// SOLUZIONE:
// 1. Include ObjectCollidable solo nei file .cpp che ne hanno bisogno
// 2. OPPURE: Crea un wrapper che rinomina il parametro

namespace VanzaKart {

// Placeholder - aggiungi qui il tuo codice

} // namespace VanzaKart
