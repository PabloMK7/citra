namespace Core {

template <class T>
T& Global();

// Declare explicit specialisation to prevent im
class System;
template <>
System& Global();

} // namespace Core
