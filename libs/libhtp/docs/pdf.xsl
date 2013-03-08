<?xml version='1.0'?> 
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:fo="http://www.w3.org/1999/XSL/Format"
    xmlns:d="http://docbook.org/ns/docbook"
    exclude-result-prefixes="d" version="1.0">
    
    <xsl:import href="./docbook-xsl-ns-1.77.1/fo/docbook.xsl" />
           
    <xsl:param name="mono.font.family">Courier</xsl:param>
    <xsl:param name="body.font.family">Times</xsl:param>
    <xsl:param name="title.font.family">Helvetica</xsl:param>   
    
    <xsl:param name="paper.type" select="A4"/>
    <xsl:param name="page.margin.inner">2.54cm</xsl:param>
    <xsl:param name="page.margin.outer">2.54cm</xsl:param>
    
    <xsl:param name="fop1.extensions" select="1"/>
    <xsl:param name="title.margin.left">0pt</xsl:param>
    
    <xsl:param name="body.font.master">11</xsl:param>
    <xsl:param name="body.start.indent">0pt</xsl:param>
    <xsl:param name="line-height">1.4</xsl:param>
    
    <xsl:param name="double.sided">1</xsl:param>

    <xsl:param name="xref.with.number.and.title">0</xsl:param>
    
    <!-- Header styling -->
    <xsl:param name="header.column.widths">1 100 1</xsl:param>
    <xsl:attribute-set name="header.content.properties">
    	<xsl:attribute name="font-size">10pt</xsl:attribute>
        <xsl:attribute name="font-family"><xsl:value-of select="$title.font.family" /></xsl:attribute>
        <xsl:attribute name="line-height">20pt</xsl:attribute>
    </xsl:attribute-set>    
    
    <!-- Footer styling -->
    <xsl:param name="footer.column.widths">100 1 100</xsl:param>
    <xsl:attribute-set name="footer.content.properties">
        <xsl:attribute name="font-size">9pt</xsl:attribute>
        <xsl:attribute name="font-family"><xsl:value-of select="$title.font.family" /></xsl:attribute>
    </xsl:attribute-set>
    
    <!-- Monospaced text -->
    <xsl:attribute-set name="monospace.verbatim.properties">        	
    	<xsl:attribute name="font-family">
            <xsl:value-of select="$mono.font.family" />
        </xsl:attribute>
        <xsl:attribute name="font-size">9pt</xsl:attribute>
        <xsl:attribute name="start-indent">0.04in</xsl:attribute>
        <xsl:attribute name="end-indent">0.02in</xsl:attribute>    
    </xsl:attribute-set>
    
    <xsl:template name="inline.monoseq">
    	<xsl:param name="content">
        	<xsl:apply-templates />
        </xsl:param>
        <fo:inline font-family="{$mono.font.family}" font-size="80%">
        	<xsl:copy-of select="$content" />
        </fo:inline>
    </xsl:template>      
        
    <!-- Variable lists: render as blocks -->    
    <xsl:param name="variablelist.term.break.after">0</xsl:param>
    <xsl:param name="variablelist.as.blocks">1</xsl:param>            
    
    <xsl:attribute-set name="normal.para.spacing">
      <xsl:attribute name="space-before.optimum">0.2em</xsl:attribute>
      <xsl:attribute name="space-before.minimum">0.2em</xsl:attribute>
      <xsl:attribute name="space-before.maximum">0.2em</xsl:attribute>
      <xsl:attribute name="space-after.optimum">0.2em</xsl:attribute>
      <xsl:attribute name="space-after.minimum">0.2em</xsl:attribute>
      <xsl:attribute name="space-after.maximum">0.2em</xsl:attribute>
    </xsl:attribute-set>
    
    <xsl:attribute-set name="admonition.title.properties">
            <xsl:attribute name="font-family"><xsl:value-of select="$title.font.family" /></xsl:attribute>
            <xsl:attribute name="font-size">12pt</xsl:attribute>
            <xsl:attribute name="font-weight">bold</xsl:attribute>
            <xsl:attribute name="space-after.minimum">0pt</xsl:attribute>
            <xsl:attribute name="space-after.optimum">0pt</xsl:attribute>
            <xsl:attribute name="space-after.maximum">0pt</xsl:attribute>
    </xsl:attribute-set>
    
    <xsl:attribute-set name="section.title.properties">
      <xsl:attribute name="font-weight">bold</xsl:attribute>
      <xsl:attribute name="keep-with-next.within-column">always</xsl:attribute>
      <xsl:attribute name="text-align">left</xsl:attribute>  
      <xsl:attribute name="space-after.minimum">0.2em</xsl:attribute>
      <xsl:attribute name="space-after.optimum">0.2em</xsl:attribute>
      <xsl:attribute name="space-after.maximum">0.2em</xsl:attribute>
    </xsl:attribute-set>
    
    <xsl:attribute-set name="section.title.level1.properties">
      <xsl:attribute name="font-size">18pt</xsl:attribute>
      
    </xsl:attribute-set>
    
    <xsl:attribute-set name="section.title.level2.properties">
      <xsl:attribute name="font-size">16pt</xsl:attribute>
      <xsl:attribute name="space-before.minimum">1.0em</xsl:attribute>
      <xsl:attribute name="space-before.optimum">1.0em</xsl:attribute>
      <xsl:attribute name="space-before.maximum">1.0em</xsl:attribute> 
    </xsl:attribute-set>
    
    <xsl:attribute-set name="section.title.level3.properties">
      <xsl:attribute name="font-size">14pt</xsl:attribute>
      <xsl:attribute name="space-before.minimum">1.0em</xsl:attribute>
      <xsl:attribute name="space-before.optimum">1.0em</xsl:attribute>
      <xsl:attribute name="space-before.maximum">1.0em</xsl:attribute> 
    </xsl:attribute-set>
    
    <xsl:attribute-set name="section.title.level4.properties">
      <xsl:attribute name="font-size">12pt</xsl:attribute>
      <xsl:attribute name="space-before.minimum">1.0em</xsl:attribute>
      <xsl:attribute name="space-before.optimum">1.0em</xsl:attribute>
      <xsl:attribute name="space-before.maximum">1.0em</xsl:attribute> 
    </xsl:attribute-set>
    
    <xsl:attribute-set name="list.item.spacing">
      <xsl:attribute name="space-before.optimum">0.4em</xsl:attribute>
      <xsl:attribute name="space-before.minimum">0.4em</xsl:attribute>
      <xsl:attribute name="space-before.maximum">0.4em</xsl:attribute>
      <xsl:attribute name="space-after.optimum">0.2em</xsl:attribute>
      <xsl:attribute name="space-after.minimum">0.2em</xsl:attribute>
      <xsl:attribute name="space-after.maximum">0.2em</xsl:attribute>
    </xsl:attribute-set>
    
    <xsl:attribute-set name="list.block.spacing">
      <xsl:attribute name="space-before.optimum">0.2em</xsl:attribute>
      <xsl:attribute name="space-before.minimum">0.2em</xsl:attribute>
      <xsl:attribute name="space-before.maximum">0.2em</xsl:attribute>
    
      <xsl:attribute name="space-after.optimum">0.2em</xsl:attribute>
      <xsl:attribute name="space-after.minimum">0.2em</xsl:attribute>
      <xsl:attribute name="space-after.maximum">0.2em</xsl:attribute>
    
      <xsl:attribute name="margin-left">2pc</xsl:attribute>
    </xsl:attribute-set>
    
    <xsl:param name="shade.verbatim" select="1"/>
    <xsl:attribute-set name="shade.verbatim.style">
        <xsl:attribute name="background-color">#f7fbff</xsl:attribute>
        <xsl:attribute name="padding">6pt</xsl:attribute>
    </xsl:attribute-set>
    
    <xsl:template name="nongraphical.admonition">
      <xsl:variable name="id">
        <xsl:call-template name="object.id"/>
      </xsl:variable>
    
      <fo:block space-before.minimum="0.8em"
                space-before.optimum="1em"
                space-before.maximum="1.2em"
                start-indent="1in"
                end-indent="1in"
                border-top="0.5pt solid black"
                border-bottom="0.5pt solid black"
                padding-top="4pt"
                padding-bottom="2pt"
                id="{$id}">
        <xsl:if test="$admon.textlabel != 0 or title">
          <fo:block keep-with-next='always'
                    xsl:use-attribute-sets="admonition.title.properties">
             <xsl:apply-templates select="." mode="object.title.markup"/>
          </fo:block>
        </xsl:if>
    
        <fo:block xsl:use-attribute-sets="admonition.properties">
          <xsl:apply-templates/>
        </fo:block>
      </fo:block>
    </xsl:template>

    <!--    
    <xsl:template name="book.titlepage.before.recto">
    	<fo:block-container>
            <fo:table table-layout="fixed" width="100%">
                <fo:table-column column-width="proportional-column-width(1)"/>
                    <fo:table-body>
                        <fo:table-row>
                            <fo:table-cell display-align="center">
                            <fo:block text-align="center" font-size="60pt" line-height="50pt"><fo:basic-link external-destination="url(http://www.ironbee.com)">
                                <fo:external-graphic content-width="8cm" src="url(ironbee-logo.png)"/>
                             </fo:basic-link></fo:block>
                            </fo:table-cell>
                        </fo:table-row>                    
                    </fo:table-body>
                </fo:table>
            </fo:block-container>
    </xsl:template>
    
    <xsl:template name="book.titlepage.before.verso">        
        <fo:block-container absolute-position="fixed" top="200mm">
            <fo:table table-layout="fixed" width="100%" break-after="page">
                <fo:table-column column-width="proportional-column-width(1)"/>
                    <fo:table-body>
                        <fo:table-row>
                            <fo:table-cell display-align="center">
                                <fo:block text-align="center" font-size="60pt" line-height="50pt"><fo:basic-link external-destination="url(http://www.ironbee.com)">
                                    <fo:external-graphic content-width="3.5cm" src="url(ironbee-logo.png)"/>
                                </fo:basic-link></fo:block>	
                            </fo:table-cell>
                        </fo:table-row>                    
                    </fo:table-body>
                </fo:table>
            </fo:block-container>                
        <fo:block xmlns:fo="http://www.w3.org/1999/XSL/Format" break-after="page" />                
    </xsl:template>
    -->
    
    <xsl:attribute-set name="toc.line.properties">        
    	<xsl:attribute name="font-size">10pt</xsl:attribute>        
        <xsl:attribute name="line-height">14pt</xsl:attribute>
        <xsl:attribute name="font-family"><xsl:value-of select="$title.font.family" /></xsl:attribute>
        <xsl:attribute name="font-weight">
        <xsl:choose>
        	<xsl:when test="self::d:chapter | self::d:preface | self::d:appendix | self::d:index">bold</xsl:when>
            <xsl:otherwise>normal</xsl:otherwise>
        </xsl:choose>
        </xsl:attribute>        
    </xsl:attribute-set>  
    
    <xsl:template name="toc.line">
            <xsl:param name="toc-context" select="NOTANODE"/>
            
            <xsl:variable name="id">
                <xsl:call-template name="object.id"/>
            </xsl:variable>
            
            <xsl:variable name="label">
                <xsl:apply-templates select="." mode="label.markup"/>
            </xsl:variable>
            
            <fo:block xsl:use-attribute-sets="toc.line.properties">
                <fo:inline keep-with-next.within-line="always">
                    <fo:basic-link internal-destination="{$id}">
                        <xsl:if test="$label != ''">
                            <xsl:copy-of select="$label"/>
                            <xsl:value-of select="$autotoc.label.separator"/>
                        </xsl:if>
                        <xsl:apply-templates select="." mode="titleabbrev.markup"/>
                    </fo:basic-link>
                </fo:inline>
                <fo:inline keep-together.within-line="always">
                    <xsl:text> </xsl:text>
                    <xsl:choose>
                        <xsl:when test="self::d:chapter | self::d:preface | self::d:appendix | self::d:index">
                            <fo:leader leader-pattern="dots"
                                leader-pattern-width="4pt"
                                leader-alignment="reference-area"
                                keep-with-next.within-line="always"/>                                 
                            <xsl:text> </xsl:text>
                        </xsl:when>                    
                        <xsl:otherwise>
                            <fo:leader leader-pattern="space"
                                leader-pattern-width="4pt"
                                leader-alignment="reference-area"
                                keep-with-next.within-line="always"/>                                 
                            <xsl:text> </xsl:text>
                        </xsl:otherwise>
                    </xsl:choose>    
                    <xsl:text> </xsl:text>
                    <fo:basic-link internal-destination="{$id}">
                        <fo:page-number-citation ref-id="{$id}"/>
                    </fo:basic-link>
                </fo:inline>
            </fo:block>
        </xsl:template>
        
    <xsl:param name="generate.toc">appendix title article/appendix nop article toc,title book
            toc,title,example,equation chapter title part title preface title qandadiv toc qandaset
            toc reference toc,title sect1 toc sect2 toc sect3 toc sect4 toc sect5 toc section toc set
            toc,title</xsl:param>          
    
    <xsl:param name="local.l10n.xml" select="document('')" />
    <l:i18n xmlns:l="http://docbook.sourceforge.net/xmlns/l10n/1.0">
    	<l:l10n language="en">
    		<l:context name="title-numbered">
        		<l:template name="part" text="Part %n: %t" />
    	        <l:template name="chapter" text="Chapter %n: %t" />
        	    <l:template name="appendix" text="Appendix %n: %t" />
        	</l:context>    
    	</l:l10n>
    </l:i18n> 

</xsl:stylesheet>
