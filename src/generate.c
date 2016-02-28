#include "game.h"
#include "generate.h"

//Hints Format [0]kNoHint [1]kShield [2]kNumber [3]kSpell [4]kGreek
// kNoHint is currently unused, any other bit true will indicate that reqHint is true
// Set levels which are not to be chosen at random to minL = 9
RoomDescriptor_t m_roomDescriptor[kNRoomTypes] = {
 [kStart].m_minL  = 0, [kStart].m_giveHint  = 1, [kStart].m_giveItem  = 0, [kStart].m_reqItem  = 0, [kStart].m_reqHint  = {0, 0, 0, 0, 0},
 [kStairs].m_minL = 0, [kStairs].m_giveHint = 1, [kStairs].m_giveItem = 0, [kStairs].m_reqItem = 0, [kStairs].m_reqHint = {0, 0, 0, 0, 0},
 [kWin].m_minL    = 9, [kWin].m_giveHint    = 1, [kWin].m_giveItem    = 0, [kWin].m_reqItem    = 0, [kWin].m_reqHint    = {0, 0, 0, 0, 0},
 [kLoose].m_minL  = 9, [kLoose].m_giveHint  = 1, [kLoose].m_giveItem  = 0, [kLoose].m_reqItem  = 0, [kLoose].m_reqHint  = {0, 0, 0, 0, 0}
};

int m_hintsInPlay = 0;
uint8_t m_hintIsActive[kNHintTypes] = {0};
uint8_t m_hintValue[kNHintTypes] = {0};

int m_itemsInPlay = 0;
uint8_t m_itemValue[kNItemTypes] = {0};

uint8_t getHintValue(Hints_t _hint) {
  switch (_hint) {
    case kShield: return rand() % kNShieldTypes;
    case kNumber: return rand() % MAX_NUMBER;
    case kSpell: return rand() % MAX_SPELLS;
    case kGreek: return rand() % MAX_GREEK;
    default: return 0;
  }
}

uint8_t getItemValue() {
  while (true) {
    Items_t _item = rand() % kNItemTypes;
    if (_item == kNoItem) continue;
    if (m_itemsInPlay == 1 && m_itemValue[0] == _item) continue; // TODO generalise to avoid duplicates if player is allowed more then 2 items
    return _item;
  }
}

Hints_t getHint(Rooms_t _room) {
  if (m_roomDescriptor[_room].m_giveHint == 0) return kNoHint; // Room does not allow hints
  if (m_hintsInPlay == HINT_TYPES) return kNoHint; // Have max hints in play
  if (rand() % 100 > HINT_CHANCE) return kNoHint; // rnd

  // Else choose a hint
  Hints_t _hint = kNoHint;
  while (_hint == kNoHint) {
    Hints_t _random = rand() % kNHintTypes;
    if ( m_hintIsActive[_random] == false ) _hint = _random;
  }

  return _hint;
}



Rooms_t getRoom(int _level, Hints_t* _consumeHint, bool* _consumeItem) {

  // TODO make rooms be spread out


  while (true) {
    Rooms_t _room = rand() % kNRoomTypes;

    // Check level
    if (m_roomDescriptor[_room].m_minL > _level) continue;

    // Check if adds item, that room is free
    if (m_roomDescriptor[_room].m_giveItem == 1 && m_itemsInPlay == MAX_INVENTORY) continue;

    // Check if needs hint
    bool _needsHint = false;
    int _nCompatableHints = 0;
    int _hints[kNHintTypes] = {0};
    for (Hints_t _h = 1; _h < kNHintTypes; ++_h) { // Avoid kNoHint
      if ( m_roomDescriptor[_room].m_reqHint[ _h ] == 1 ) _needsHint = true; // I require a hint
      if ( m_roomDescriptor[_room].m_reqHint[ _h ] == 1 && m_hintIsActive[ _h ] == 1) { // This hint is available
        _hints[ _nCompatableHints++ ] = _h;
      }
    }
    if (_needsHint == true && _nCompatableHints == 0) continue; // Roll again
    else if (_needsHint == true) (*_consumeHint) = _hints[ rand() % _nCompatableHints ]; // Consume an available hint

    // Check if needs item
    bool _needsItem = m_roomDescriptor[_room].m_reqItem;
    if (_needsItem == true && m_itemsInPlay == 0) continue; // Roll again
    else if (_needsItem == true) (*_consumeItem) = true; // Consume an item

    return _room;
  }
}

void generate() {

  m_dungeon.m_seed = time(NULL);
  srand(m_dungeon.m_seed);

  m_dungeon.m_level = 0;
  m_dungeon.m_room = -1; // Will be incremented on kNewLevel
  m_dungeon.m_lives = 1;


  for (int _level = 0; _level < MAX_LEVELS; ++ _level) {
    m_dungeon.m_roomsPerLevel[_level] = MIN_ROOMS + (rand() % (MAX_ROOMS - MIN_ROOMS));
    APP_LOG(APP_LOG_LEVEL_INFO," ------------> LEVEL %i, N ROOMS %i", _level, m_dungeon.m_roomsPerLevel[_level]);
    for (int _room = 0; _room < m_dungeon.m_roomsPerLevel[_level]; ++_room) {
      APP_LOG(APP_LOG_LEVEL_INFO,"-----");

      Hints_t _consumeHint = kNoHint;
      bool _consumeItem = false;
      Rooms_t _roomType = getRoom(_level, &_consumeHint, &_consumeItem);
      m_dungeon.m_rooms[_level][_room] = _roomType;
      APP_LOG(APP_LOG_LEVEL_INFO,"Generating Level:%i Room:%i = type %i", _level, _room, (int)_roomType);

      // Can we add a hint to this room?
      Hints_t _addHint = getHint(_roomType);
      if (_addHint != kNoHint) {
        ++m_hintsInPlay;
        m_hintIsActive[_addHint] = 1;
        m_hintValue[_addHint] = getHintValue(_addHint);

        m_dungeon.m_roomGiveHint[_level][_room] = _addHint;
        m_dungeon.m_roomGiveHintValue[_level][_room] = m_hintValue[_addHint];
        APP_LOG(APP_LOG_LEVEL_INFO,"  ADDING *hint* type %i, value %i", (int)_addHint, (int)m_hintValue[_addHint]);

      }

      // Are we consuming a hint?
      if (_consumeHint != kNoHint) {
        m_dungeon.m_roomNeedHint[_level][_room] = _consumeHint;
        m_dungeon.m_roomNeedHintValue[_level][_room] = m_hintValue[_consumeHint];
        APP_LOG(APP_LOG_LEVEL_INFO,"  CONSUMING *hint* type %i, value %i", (int)_consumeHint, (int)m_hintValue[_consumeHint]);

        --m_hintsInPlay;
        m_hintIsActive[_consumeHint] = 0;
        m_hintValue[_consumeHint] = 0;
      }

      // Note - it is assumed we will never add and consume an item

      // Are we adding an item?
      if (m_roomDescriptor[_roomType].m_giveItem == 1) {
        m_itemValue[ m_itemsInPlay ] = getItemValue();
        m_dungeon.m_roomGiveItem[_level][_room] = m_itemValue[ m_itemsInPlay ];
        APP_LOG(APP_LOG_LEVEL_INFO,"  ADDING #item# type %i", (int)m_itemValue[ m_itemsInPlay ]);
        ++m_itemsInPlay;
      }

      //Are we taking an item?
      if (_consumeItem == true) {
        int _toTake = rand() % m_itemsInPlay; // If there are 2, choose which to consume. itemsInPlay is 1 or 2 here
        m_dungeon.m_roomNeedItem[_level][_room] = m_itemValue[ _toTake ];
        APP_LOG(APP_LOG_LEVEL_INFO,"  CONSUMING #item# type %i", (int)m_itemValue[ _toTake ]);

        // TODO this also assumes we will only ever have 2 items
        // Move other down
        if (_toTake == 0 && m_itemsInPlay == 2) m_itemValue[0] = m_itemValue[1];
        --m_itemsInPlay;
      }

    }
  }
  m_gameState = kNewRoom;
}