#!/usr/bin/perl -w

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

# {{{ program header

use strict;

use XML::GDOME;

# forwards
sub treat_dir($);
sub treat_file($);
sub convert_message_element($$$$);

# }}}

if(@ARGV < 1) {
  print "Usage:\n\t$0 [xmlfile|directory]\n";
  exit 0;
}

foreach(@ARGV) {
  if(-d $_) {
    treat_dir($_);
  }
  else {
    treat_file($_);
  }
}

# {{{ treat_dir
sub treat_dir($) {
  my $directory = shift;

  opendir DIR,$directory or die $!;

  foreach(readdir DIR) {
    next if $_ eq '.' or $_ eq '..';

    if(-d "$directory/$_") {
      treat_dir("$directory/$_");
      next;
    }

    next unless /\.xml$/;

    treat_file("$directory/$_");
  }

  closedir DIR;
}
# }}}

# {{{ treat_file
sub treat_file($) {
  my $original_document = XML::GDOME->createDocFromURI($_[0],GDOME_LOAD_SUBSTITUTE_ENTITIES);
  my $new_document_type = XML::GDOME->createDocumentType("Forum",undef,"http://wwwtech.de/cforum/download/cforum-3.dtd");
  my $new_document = XML::GDOME->createDocument(undef,"Forum",$new_document_type);

  my $nodelist = $original_document->getElementsByTagName("ContentList");
  my %messages;
  if ($nodelist->getLength()) {
    my $contentlist_node = $nodelist->item(0);
    $nodelist = $contentlist_node->getChildNodes();
    for(my $i = 0; $i < $nodelist->getLength(); $i++) {
      my $content_node = $nodelist->item($i);
      next unless $content_node->hasAttributes();
      my $mid = $content_node->getAttribute("mid");
      next unless $mid;
      next unless $content_node->getChildNodes()->getLength() == 1;
      next unless $content_node->getChildNodes()->item(0)->getNodeType() == CDATA_SECTION_NODE;
      $messages{$mid} = $new_document->importNode($content_node->getChildNodes()->item(0),1);

      repair_data($messages{$mid},$new_document);
    }
  }

  my $original_document_element = $original_document->getDocumentElement();
  my $new_document_element = $new_document->getDocumentElement();

  $new_document_element->setAttribute("lastThread", $original_document_element->getAttribute("lastThread")) if $original_document_element->getAttribute("lastThread") ne "";
  $new_document_element->setAttribute("lastMessage", $original_document_element->getAttribute("lastMessage")) if  $original_document_element->getAttribute("lastMessage") ne "";
  
  $nodelist = $original_document->getElementsByTagName("Thread");
  for(my $i = 0; $i < $nodelist->getLength(); $i++) {
    my $node = $nodelist->item($i);
    next unless $node->getAttribute("id") ne "";
    my $nc = $new_document->importNode($node,0);
    $new_document_element->appendChild($nc);
    $nc->setAttribute("id",$node->getAttribute("id"));
    convert_message_element($node,$nc,$new_document,\%messages);
  }
  open DAT,'>'.$_[0] or return;
  print DAT $new_document->toStringEnc("UTF-8",GDOME_SAVE_STANDARD);
  close DAT;
  return;
}
# }}}

# {{{ convert_message_element
sub convert_message_element($$$$) {
  my ($original_parent,$new_parent,$new_document,$messages) = @_;
  my $original_children = $original_parent->getChildNodes();
  my @new_nodelist;
  
  # first: just copy <Header> and save other <Message> elements
  for(my $i = 0; $i < $original_children->getLength(); $i++) {
    my $node = $original_children->item($i);
    next unless $node->getNodeType() == ELEMENT_NODE;
    if($node->getTagName() eq "Header") {
      my $new_child_node = $new_document->importNode($node,($node->getTagName() eq "Message" ? 0 : 1));
      my $flag_nodes = $new_child_node->getElementsByTagName("Flags");
      if(!$flag_nodes->getLength()) {
        my $flag_node = $new_document->createElement("Flags");
	$new_child_node->appendChild($flag_node);
      }
      $new_parent->appendChild($new_child_node);
      # next next statement is executed, so no need to break here
    }
    next if($node->getTagName() ne "Message");
    push @new_nodelist,$node;
  }
  
  # next: if this is a message element
  if($new_parent->getTagName() eq "Message") {
    # try to add message content if available
    my $current_mid = $new_parent->getAttribute("id");
    if($current_mid && $messages->{$current_mid}) {
      my $messagecontent = $new_document->createElement("MessageContent");
      $new_parent->appendChild($messagecontent);
      $messagecontent->appendChild($messages->{$current_mid});
    }
  }
  
  # add remaining submessages
  foreach(@new_nodelist) {
    next unless $_->getAttribute("id");
    
    my $new_child_node = $new_document->createElement("Message");
    $new_parent->appendChild($new_child_node);
    
    $new_child_node->setAttribute("id",$_->getAttribute("id"));
    $new_child_node->setAttribute("invisible",$_->getAttribute("invisible")) if $_->getAttribute("invisible") ne "";
    $new_child_node->setAttribute("ip",$_->getAttribute("ip")) if $_->getAttribute("ip") ne "";
    $new_child_node->setAttribute("unid",$_->getAttribute("unid")) if $_->getAttribute("unid") ne "";
    $new_child_node->setAttribute("votingGood",$_->getAttribute("votingGood")) if $_->getAttribute("votingGood") ne "";
    $new_child_node->setAttribute("votingBad",$_->getAttribute("votingBad")) if $_->getAttribute("votingBad") ne "";
    convert_message_element($_,$new_child_node,$new_document,$messages);
  }
}
# }}}

# {{{ repair_data
sub repair_data {
  my $node = shift;
  my $doc  = shift;
  my $data = $node->getFirstChild ? $node->getFirstChild->getNodeValue : $node->getNodeValue;

  return unless $data;

  $data =~ s!<a href="([^"]+)">.*?</a>![link:$1]!g;
  $data =~ s!<img src="([^"]+)"(?: alt="([^"]+)")? ?/?>!"[image:".$1.($2?'@alt='.$2:'')."]"!eg;
  $data =~ s!<iframe src="([^"]+)">.*?</iframe>![iframe:$1]!g;

  if($node->getFirstChild) {
    my $x = $doc->createTextNode($data);
    $node->replaceChild($x,$node->getFirstChild);
  }
  else {
    $node->setNodeValue($data);
  }
}
# }}}

# eof
