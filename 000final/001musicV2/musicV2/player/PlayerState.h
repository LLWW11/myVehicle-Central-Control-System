#ifndef PLAYERSTATE_H
#define PLAYERSTATE_H

#include <QMetaType>

enum class PlayerState { Stopped, Playing, Paused };

Q_DECLARE_METATYPE(PlayerState)

#endif // PLAYERSTATE_H
