MEMORY
{
	mem0 : ORIGIN = 0, LENGTH = 0x4000006
	mem1 : ORIGIN = 0x5000000, LENGTH = 0x8000000
}

SECTIONS
{
	mem : {
		.ANY
	} > mem0
	
	mem1 : {
		.ANY
	} > mem1
}
