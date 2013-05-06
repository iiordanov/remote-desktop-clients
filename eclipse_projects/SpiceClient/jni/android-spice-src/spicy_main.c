int spice_main(char* cmd);
int main()
{
    return spice_main("./spicy -h 192.168.2.31 -p 5902 -w gnoll");
}

