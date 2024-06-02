 #if WITH_LANDLOCK

/*
** remove write and execute permissions
*/
void landlock_drop_write (void);

/*
** restrict write permissions to XDG_DATA
*/
void landlock_restrict_write (void);

#endif
