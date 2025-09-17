import file;
DATA = file("chipdata.cid","r");
FOUND = false;
BUF = str();
CHIP = tup(str(), str(), str());
LIST = tab(0, CHIP);
while DATA.readln(BUF) loop
    while ((BUF.count() > 0) and (BUF.at((BUF.count() - 1)) < 33)) loop
        BUF = substr(BUF, 0, (BUF.count() - 1));
    end loop;
    TOKENS = tokenize(BUF, "=", true);
    if (TOKENS.count() < 2) then
        FOUND = false;
        continue;
    end if;
    VAR = TOKENS.at(0);
    VAL = TOKENS.at(1);
    if (not FOUND and (upper(VAR) == "CHIPNAME")) then
        if not isnull(CHIP@1) then
            LIST.concat(CHIP);
        end if;
        CHIP.set@1(VAL);
        FOUND = true;
    elsif not FOUND then
        continue;
    elsif (VAR == "ProgramFlag2") then
        CHIP.set@2(VAL);
    elsif (VAR == "PanelSizing") then
        CHIP.set@3(VAL);
    end if;
end loop;
if not isnull(CHIP@1) then
    LIST.concat(CHIP);
end if;
DATA.close();
OUT2 = file("out.dat","w");
DAT2 = file("picpro.dat","r");
INCHIP = tup(str(), str(), str());
CC = 0;
FC = 0;
LIST_NOTFOUND = tab(0, "");
LIST_OUT = tab(0, "");
FOUND = false;
while DAT2.readln(BUF) loop
    while ((BUF.count() > 0) and (BUF.at((BUF.count() - 1)) < 33)) loop
        BUF = substr(BUF, 0, (BUF.count() - 1));
    end loop;
    TOKENS = tokenize(BUF, "=", true);
    if (TOKENS.count() < 2) then
        FOUND = false;
        OUT2.write((BUF + "\n"));
        continue;
    end if;
    VAR = TOKENS.at(0);
    VAL = TOKENS.at(1);
    if (not FOUND and (upper(VAR) == "CHIPNAME")) then
        forall E in LIST loop
            if (E@1 == VAL) then
                INCHIP = E;
                FOUND = true;
                FC = (FC + 1);
                break;
            end if;
        end loop;
        CC = (CC + 1);
        if not FOUND then
            LIST_NOTFOUND.concat(VAL);
        end if;
        LIST_OUT.concat(VAL);
        OUT2.write((BUF + "\n"));
    elsif FOUND then
        if (VAR == "ProgramTries") then
            OUT2.write((("ProgramTries=" + INCHIP@2) + "\n"));
        elsif (VAR == "OverProgram") then
            OUT2.write((("PanelSizing=" + str(int(INCHIP@3))) + "\n"));
        else
            OUT2.write((BUF + "\n"));
        end if;
    else
        OUT2.write((BUF + "\n"));
    end if;
end loop;
DAT2.close();
OUT2.close();
print "IN COUNT: " LIST.count() " , FOUND: " FC " , OUT: " CC;
forall E in LIST_NOTFOUND loop
    print "ADDED: " E;
end loop;
forall E in LIST loop
    EXISTS = false;
    forall S in LIST_OUT loop
        if (E@1 == S) then
            EXISTS = true;
            break;
        end if;
    end loop;
    if not EXISTS then
        print "MISSED : " E;
    end if;
end loop;
