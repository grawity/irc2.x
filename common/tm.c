main()
{
	testm("a*a", "aaa");
	testm("a*a", "a*a");
	testm("a*a", "Aaa");
	testm("a\\*a", "aaa");
	testm("a\\*a", "a*a");
	testm("a?a", "aaa");
	testm("a?a", "aaa");
	testm("a?a", "a?a");
	testm("a\\?a", "aaa");
	testm("a\\?a", "a?a");
	testm("*", "*\\**");
	testm("*\\**", "*");
	testm("*", "a\\*a");
}

testm(msk, nam)
char *msk, *nam;
{
	printf("Mask = [%s] String = [%s] = %d\n",
		msk, nam, matches(msk, nam));
}

outofmemory()
{
}
