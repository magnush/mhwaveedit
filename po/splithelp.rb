#! /usr/bin/env ruby

# Ruby script to split up translations for mhwaveedit help pages
# Syntax: ruby splithelp.rb pofile pofile2 ...

require 'pathname'

Encoding.default_external="ASCII-8BIT"


# Process a PO file. 
# Yields to block once for each entry in the PO file.

def poproc(fn)
  p = Pathname.new(fn);
  pt = p.sub_ext(".tmp");
  p.rename(pt);
  
  tc = 0
  lines = Array.new
  
  x = p.open("w");

  pt.each_line do |l|
    if l[/\A\s*\z/] # empty line or whitespace only
      if lines.length > 0
        yield(lines,x,tc);
      	tc = tc+1
      	lines = Array.new
      end
    else
      lines << l;
    end
  end
  if lines.length > 0 
    yield(lines,x,tc)
  end
  
  x.close
  pt.unlink
  tc  
end

# Takes an array of lines and returns an array of array of lines
# Each sub-array contains lines correspoding to one line of translation 
# text (the last one contains a newline character). Empty lines are skipped
def linegroup(la)
  r = Array.new
  g = Array.new
  la.each do |l|
    g << l
    if l.strip.end_with?('\n"')
      if g.length > 1 or g[0].strip != '"\n"'
        r << g
      end
      g = Array.new
    end
  end
  if g.length > 0
    r << g
  end
  return r
end


ARGV.each do |fn|   
  scount = 0
  e=poproc(fn) do |l,f,c|
    hsline = l.grep(/\A\#: src\/help.c:/)    
    idl = l.find_index("msgid \"\"\n")
    stl = l.find_index("msgstr \"\"\n")
    # puts "hsline:#{hsline} idl:#{idl} stl:#{stl}"

    if c==0 then
      # Pass on header unchanged
      l.each do |x| f.puts x end
    elsif hsline.length>0 and stl==l.length-1 then
      # Empty message - skip
      puts "Skipping untranslated entry #{hsline[0].strip}"
    elsif hsline.length>0 and idl and stl then
      # This PO entry should be processed

      # Group lines of translation together 
      idlg = linegroup(l[idl+1...stl])
      stlg = linegroup(l[stl+1..-1])
      
      # If there are more lines in translation, include all trailing 
      # lines in the last entry
      if stlg.length > idlg.length
        stlg[idlg.length-1] = stlg[(idlg.length-1) .. -1].flatten
        stlg = stlg[1...idlg.length]
      end
      # If there are more lines in original than in translation, 
      # create empty translation.
      while stlg.length < idlg.length
        stlg << [ "\" \\n\"\n" ]
      end
      
      for i in 0...idlg.length do
        f.puts ""
        l[0...idl].each { |x| f.puts(x) }
        if idlg[i].length > 1 
          f.puts 'msgid ""'
          idlg[i].each do |x| f.puts x end
        else
          f.puts "msgid #{idlg[i][0]}"
        end        
        if stlg[i].length > 1
          f.puts 'msgstr ""'
          stlg[i].each do |x| f.puts x end
        else
          f.puts "msgstr #{stlg[i][0]}"
        end
      end

      puts "Split entry #{hsline[0].strip} into #{idlg.length} entries"
      scount = scount+1
    else
      # Pass on entry unchanged
      f.puts ""
      l.each do |x| f.puts x end
    end
  end
  puts "#{fn}: #{scount} entries were split (#{e} total)"
end
