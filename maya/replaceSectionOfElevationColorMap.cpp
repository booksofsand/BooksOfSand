/* MM: This is trivial, it's just that there's a section of code
   in ElevationColorMap.cpp with an if-else where the the two blocks of code
   are almost exactly the same and it bugs me. 

   So I want to replace it with this, which still has some silly if statements
   but at least it is twice as short!
*/

{
		if(Misc::hasCaseExtension(heightMapName,".cpt"))
			heightMapSource.setPunctuation("\n");
		else
			heightMapSource.setPunctuation(",\n");		
		heightMapSource.skipWs();
		int line=1;
		while(!heightMapSource.eof())
			{
			/* Read the next color map key value: */
			heightMapKeys.push_back(GLdouble(heightMapSource.readNumber()));
			if(!Misc::hasCaseExtension(heightMapName,".cpt") && !heightMapSource.isLiteral(','))
				Misc::throwStdErr("ElevationColorMap: Color map format error in line %d of file %s",line,fullHeightMapName.c_str());

			
			/* Read the next color map color value: */
			Color color;
			for(int i=0;i<3;++i)
				if(Misc::hasCaseExtension(heightMapName,".cpt"))
					color[i]=Color::Scalar(heightMapSource.readNumber()/255.0);
				else
					color[i]=Color::Scalar(heightMapSource.readNumber());
			color[3]=Color::Scalar(1);
			heightMapColors.push_back(color);
			
			if(!heightMapSource.isLiteral('\n'))
				Misc::throwStdErr("ElevationColorMap: Color map format error in line %d of file %s",line,fullHeightMapName.c_str());
			++line;
			}
		}
