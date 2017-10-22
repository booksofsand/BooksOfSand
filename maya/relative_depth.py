import sys

row_count = 0
the_file = open(sys.argv[1], "r")
for line in the_file:
    #print(line)

    if row_count == 0:
        row_count = 1
        print(line)
        continue     # don't modify first line
    
    line = line.split("\t")
    col_count = 0
    for item in line:
        if col_count == 0:
            col_count = 1
            print(item, end="\t")
            continue       # don't modify first col
        try:
            edited_item = round(float(item) - 600, 2)
            print(edited_item, end="\t")
        except ValueError:
            pass
    print("")

the_file.close()
